"""Google OAuth 2.0 authentication + server-side sessions.

Auth for the internal members-only area. MkDocs builds to static HTML, and
static files can't gate themselves, so the gate lives here in the API: Google
signs the user in, we verify they're an @carboncopies.org account, and we hand
back an opaque session cookie backed by a `sessions` row in Postgres (so logout
and revocation actually work — the cookie holds only the random token, never
user data).
"""
import secrets
from datetime import datetime, timedelta, timezone
from typing import Optional

from fastapi import APIRouter, Cookie, Depends, HTTPException, Request
from fastapi.responses import RedirectResponse
from authlib.integrations.starlette_client import OAuth, OAuthError

from config import settings
from database import get_db, return_db, DBConnectionError

router = APIRouter(prefix="/auth", tags=["auth"])

SESSION_COOKIE = "ccf_session"

# OAuth client. Metadata (endpoints, JWKS) is fetched lazily from Google's
# OpenID discovery document on first use, so authorize_access_token() can
# verify and parse the id_token for us.
oauth = OAuth()
oauth.register(
    name="google",
    client_id=settings.GOOGLE_CLIENT_ID,
    client_secret=settings.GOOGLE_CLIENT_SECRET,
    server_metadata_url="https://accounts.google.com/.well-known/openid-configuration",
    client_kwargs={"scope": "openid email profile"},
)


# --- session storage (Postgres) -------------------------------------------
def _create_session(email: str, name: str, picture: str) -> str:
    token = secrets.token_urlsafe(32)
    expires_at = datetime.now(timezone.utc) + timedelta(seconds=settings.SESSION_TTL_SECONDS)
    conn = get_db()
    try:
        with conn.cursor() as cur:
            cur.execute(
                """INSERT INTO sessions (token, email, name, picture, expires_at)
                   VALUES (%s, %s, %s, %s, %s)""",
                (token, email, name, picture, expires_at),
            )
            conn.commit()
    finally:
        return_db(conn)
    return token


def _get_session(token: str) -> Optional[dict]:
    conn = get_db()
    try:
        with conn.cursor() as cur:
            cur.execute(
                "SELECT email, name, picture, expires_at FROM sessions WHERE token = %s",
                (token,),
            )
            row = cur.fetchone()
    finally:
        return_db(conn)
    if not row:
        return None
    email, name, picture, expires_at = row
    if expires_at <= datetime.now(timezone.utc):
        _delete_session(token)
        return None
    return {"email": email, "name": name, "picture": picture}


def _delete_session(token: str) -> None:
    conn = get_db()
    try:
        with conn.cursor() as cur:
            cur.execute("DELETE FROM sessions WHERE token = %s", (token,))
            conn.commit()
    finally:
        return_db(conn)


# --- dependencies ---------------------------------------------------------
def require_user(ccf_session: Optional[str] = Cookie(default=None)) -> dict:
    """Return the logged-in user, or raise 401. Use to gate API endpoints."""
    if not ccf_session:
        raise HTTPException(status_code=401, detail="Not authenticated")
    try:
        user = _get_session(ccf_session)
    except DBConnectionError:
        raise HTTPException(status_code=503, detail="Auth store unavailable")
    if not user:
        raise HTTPException(status_code=401, detail="Session expired or invalid")
    return user


def get_optional_user(ccf_session: Optional[str] = Cookie(default=None)) -> Optional[dict]:
    """Return the logged-in user or None (no error). Use to gate HTML pages that
    should redirect to login rather than return 401."""
    if not ccf_session:
        return None
    try:
        return _get_session(ccf_session)
    except DBConnectionError:
        return None


# --- routes ---------------------------------------------------------------
@router.get("/login")
async def login(request: Request, next: str = "/internal"):
    """Kick off the Google OAuth flow; `next` is where to land after login."""
    request.session["post_login_redirect"] = next
    return await oauth.google.authorize_redirect(request, settings.OAUTH_REDIRECT_URI)


@router.get("/callback")
async def callback(request: Request):
    """OAuth redirect target: exchange the code, verify the account, set session."""
    try:
        token = await oauth.google.authorize_access_token(request)
    except OAuthError as e:
        raise HTTPException(status_code=400, detail=f"OAuth error: {e.error}")

    userinfo = token.get("userinfo") or {}
    email = (userinfo.get("email") or "").lower()
    # Even with an "Internal" consent screen, verify server-side.
    if not userinfo.get("email_verified") or not email.endswith("@" + settings.ALLOWED_DOMAIN):
        raise HTTPException(status_code=403, detail="Account not permitted")

    session_token = _create_session(email, userinfo.get("name", ""), userinfo.get("picture", ""))
    next_url = request.session.pop("post_login_redirect", "/internal")
    resp = RedirectResponse(url=next_url, status_code=302)
    resp.set_cookie(
        SESSION_COOKIE,
        session_token,
        max_age=settings.SESSION_TTL_SECONDS,
        httponly=True,
        secure=settings.COOKIE_SECURE,
        samesite="lax",
        path="/",
    )
    return resp


@router.get("/logout")
async def logout(ccf_session: Optional[str] = Cookie(default=None)):
    if ccf_session:
        try:
            _delete_session(ccf_session)
        except DBConnectionError:
            pass  # cookie is cleared regardless
    # Send the user back to the public site home — no interstitial page.
    resp = RedirectResponse(url=settings.SITE_URL, status_code=302)
    resp.delete_cookie(SESSION_COOKIE, path="/")
    return resp


@router.get("/me")
async def me(user: dict = Depends(require_user)):
    """Return the current user, or 401. Handy for the frontend to check auth."""
    return user
