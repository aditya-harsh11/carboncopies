"""Configuration settings"""
import os
from pathlib import Path
from dotenv import load_dotenv
from pydantic_settings import BaseSettings

# Load backend/.env (Google OAuth creds, session signing key). Explicit path so
# it loads no matter which directory uvicorn is started from.
load_dotenv(Path(__file__).resolve().parent / ".env")


class Settings(BaseSettings):
    # Database
    DB_HOST: str = os.getenv("DB_HOST", "localhost")
    DB_PORT: str = os.getenv("DB_PORT", "5432")
    DB_NAME: str = os.getenv("DB_NAME", "ccf_website")
    DB_USER: str = os.getenv("DB_USER", "chardapple")
    DB_PASSWORD: str = os.getenv("DB_PASSWORD", "")

    # Server
    APP_HOST: str = os.getenv("APP_HOST", "0.0.0.0")
    APP_PORT: int = int(os.getenv("APP_PORT", "8080"))

    # CORS — comma-separated origins in env (the MkDocs dev site). Kept as a str
    # because pydantic-settings JSON-decodes list-typed fields, which chokes on a
    # plain CSV; use cors_origins() for the parsed list. No "*": the auth cookie
    # is credentialed, and browsers reject "*" on credentialed requests.
    CORS_ORIGINS: str = os.getenv(
        "CORS_ORIGINS", "http://localhost:9000,http://127.0.0.1:9000"
    )

    # Google OAuth (internal members-area login)
    GOOGLE_CLIENT_ID: str = os.getenv("GOOGLE_CLIENT_ID", "")
    GOOGLE_CLIENT_SECRET: str = os.getenv("GOOGLE_CLIENT_SECRET", "")
    OAUTH_REDIRECT_URI: str = os.getenv(
        "OAUTH_REDIRECT_URI", "http://localhost:8080/auth/callback"
    )
    # Only accounts on this Workspace domain may sign in (defence-in-depth on top
    # of the "Internal" OAuth consent screen).
    ALLOWED_DOMAIN: str = os.getenv("ALLOWED_DOMAIN", "carboncopies.org")

    # Where to send users after logout — the public site home. Same-domain "/" in
    # prod; the dev MkDocs site runs on :9000.
    SITE_URL: str = os.getenv("SITE_URL", "/")

    # Sessions
    SESSION_SECRET: str = os.getenv("SESSION_SECRET", "dev-only-insecure-secret-change-me")
    SESSION_TTL_SECONDS: int = int(os.getenv("SESSION_TTL_SECONDS", str(8 * 60 * 60)))
    # True in production (HTTPS) so the cookie is Secure-only; False for http dev.
    COOKIE_SECURE: bool = os.getenv("COOKIE_SECURE", "false").lower() == "true"

    def cors_origins(self) -> list[str]:
        """Parsed CORS allow-list (CORS_ORIGINS is a comma-separated string)."""
        return [o.strip() for o in self.CORS_ORIGINS.split(",") if o.strip()]


settings = Settings()
