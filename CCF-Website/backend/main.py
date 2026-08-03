"""Carboncopies Team API - FastAPI Backend Service"""
from fastapi import FastAPI, HTTPException, Query, Depends
from fastapi.middleware.cors import CORSMiddleware
from typing import List, Optional
from datetime import datetime
import os
import sys

# Add backend directory to path for imports
sys.path.append(os.path.dirname(os.path.abspath(__file__)))

from fastapi.responses import HTMLResponse, RedirectResponse
from starlette.middleware.sessions import SessionMiddleware

from models import TeamMember, TeamMemberCreate, TeamMemberUpdate, Department
from database import get_db, return_db, DBConnectionError, init_connection_pool, close_all_connections
from config import settings
from auth import router as auth_router, get_optional_user

# Initialize connection pool on startup
init_connection_pool()

app = FastAPI(
    title="Carboncopies Team API",
    description="API for managing Carboncopies team members with graceful database fallback",
    version="1.0.0",
    docs_url="/docs",
    redoc_url="/redoc"
)

# CORS — explicit origins (no "*") because the auth cookie is credentialed and
# browsers reject a wildcard origin on credentialed requests.
app.add_middleware(
    CORSMiddleware,
    allow_origins=settings.cors_origins(),
    allow_methods=["GET", "POST", "PUT", "DELETE", "OPTIONS"],
    allow_headers=["*"],
    allow_credentials=True,
    expose_headers=["*"]
)

# Signed cookie carrying only the transient OAuth state/nonce between
# /auth/login and /auth/callback (Authlib needs request.session). The logged-in
# session is a separate opaque cookie backed by Postgres (see auth.py).
app.add_middleware(
    SessionMiddleware,
    secret_key=settings.SESSION_SECRET,
    same_site="lax",
    https_only=settings.COOKIE_SECURE,
)

# Google-OAuth login routes: /auth/login, /auth/callback, /auth/logout, /auth/me
app.include_router(auth_router)


# Dependency to get DB connection
def get_db_connection():
    """Dependency that provides a database connection"""
    try:
        conn = get_db()
        yield conn
        return_db(conn)
    except DBConnectionError as e:
        # Return None if DB is unavailable - handlers will handle gracefully
        yield None


@app.on_event("shutdown")
def shutdown_event():
    """Close all database connections on shutdown"""
    close_all_connections()


@app.get("/health")
async def health_check():
    """
    Health check endpoint - always returns OK even if DB is down.
    This ensures the site doesn't crash if the database is unavailable.
    """
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("SELECT 1")
        return_db(conn)
        return {"status": "ok", "database": "connected"}
    except DBConnectionError:
        # Graceful degradation: service is up but DB is not
        return {"status": "ok", "database": "disconnected", "warning": "Database unavailable"}
    except Exception as e:
        return {"status": "ok", "database": "error", "warning": str(e)}


# Internal members area — a blank white page with an "Internal Website" header.
_INTERNAL_PAGE = """<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Internal Website</title>
<style>
  body { margin: 0; background: #fff; color: #111;
         font-family: system-ui, -apple-system, "Segoe UI", Roboto, sans-serif; }
  header { display: flex; align-items: center; justify-content: space-between;
           padding: 1.1rem 1.6rem; border-bottom: 1px solid #eaeaea; }
  header h1 { margin: 0; font-size: 1.3rem; }
  header a { font-size: 0.85rem; color: #2e279d; text-decoration: none; }
  header a:hover { text-decoration: underline; }
</style>
</head>
<body>
  <header>
    <h1>Internal Website</h1>
    <a href="/auth/logout">Log out</a>
  </header>
</body>
</html>"""


@app.get("/internal")
async def internal_home(user: Optional[dict] = Depends(get_optional_user)):
    """Gated internal members area. Unauthenticated browsers are bounced to Google
    login; authenticated @carboncopies.org users see the page."""
    if not user:
        return RedirectResponse(url="/auth/login?next=/internal", status_code=302)
    return HTMLResponse(_INTERNAL_PAGE)


@app.get("/api/team-members", response_model=List[TeamMember])
async def get_team_members(
    department: Optional[Department] = Query(None, description="Filter by department")
):
    """
    Get all team members, optionally filtered by department.
    
    **Graceful Degradation**: Returns empty array if database is unavailable.
    The site can continue to function using static content as fallback.
    """
    try:
        conn = get_db()
        query = "SELECT * FROM team_members WHERE is_active = TRUE"
        params = []
        
        if department:
            query += " AND department = %s"
            params.append(department.value)
        
        query += " ORDER BY sort_order, name"
        
        with conn.cursor() as cur:
            cur.execute(query, params)
            rows = cur.fetchall()
        
        return_db(conn)
        return [TeamMember.from_orm(row) for row in rows]
    except DBConnectionError:
        # Graceful fallback: return empty list instead of error
        print("Warning: Database unavailable, returning empty list for team members")
        return []
    except Exception as e:
        # Log but don't crash - return empty
        print(f"Error fetching team members: {e}")
        return []


# NOTE: This static route MUST be declared before the dynamic
# `/api/team-members/{member_id}` route below. FastAPI matches in declaration
# order, so if `{member_id}` (int) comes first it swallows "departments" and 422s.
@app.get("/api/team-members/departments", response_model=List[Department])
async def get_departments():
    """
    Get list of all valid departments.
    This endpoint doesn't require database access.
    """
    return list(Department)


@app.get("/api/team-members/{member_id}", response_model=Optional[TeamMember])
async def get_team_member(member_id: int):
    """
    Get a single team member by ID.
    
    **Graceful Degradation**: Returns None if database is unavailable.
    """
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("SELECT * FROM team_members WHERE id = %s AND is_active = TRUE", (member_id,))
            row = cur.fetchone()
        
        return_db(conn)
        if row:
            return TeamMember.from_orm(row)
        return None
    except DBConnectionError:
        print(f"Warning: Database unavailable, cannot retrieve team member {member_id}")
        return None
    except Exception as e:
        print(f"Error fetching team member {member_id}: {e}")
        return None


@app.post("/api/team-members", response_model=TeamMember, status_code=201)
async def create_team_member(member: TeamMemberCreate):
    """
    Create a new team member.
    
    **Note**: Returns 503 if database is unavailable (write operations require DB).
    """
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute(
                """INSERT INTO team_members 
                   (name, title, department, description, bio, linkedin_url, twitter_url, email, image_url, sort_order)
                   VALUES (%s, %s, %s, %s, %s, %s, %s, %s, %s, %s)
                   RETURNING *""",
                (
                    member.name,
                    member.title,
                    member.department.value,
                    member.description,
                    member.bio,
                    member.linkedin_url,
                    member.twitter_url,
                    member.email,
                    member.image_url,
                    member.sort_order or 0,
                )
            )
            row = cur.fetchone()
            conn.commit()
        
        return_db(conn)
        return TeamMember.from_orm(row)
    except DBConnectionError:
        raise HTTPException(
            status_code=503,
            detail="Database unavailable - cannot create team member"
        )
    except Exception as e:
        raise HTTPException(
            status_code=400,
            detail=f"Error creating team member: {str(e)}"
        )


@app.put("/api/team-members/{member_id}", response_model=Optional[TeamMember])
async def update_team_member(member_id: int, member: TeamMemberUpdate):
    """
    Update an existing team member.
    
    **Note**: Returns 503 if database is unavailable (write operations require DB).
    """
    try:
        conn = get_db()
        # Build dynamic update query
        update_fields = []
        params = []
        
        if member.name is not None:
            update_fields.append("name = %s")
            params.append(member.name)
        if member.title is not None:
            update_fields.append("title = %s")
            params.append(member.title)
        if member.department is not None:
            update_fields.append("department = %s")
            params.append(member.department.value)
        if member.description is not None:
            update_fields.append("description = %s")
            params.append(member.description)
        if member.bio is not None:
            update_fields.append("bio = %s")
            params.append(member.bio)
        if member.linkedin_url is not None:
            update_fields.append("linkedin_url = %s")
            params.append(member.linkedin_url)
        if member.twitter_url is not None:
            update_fields.append("twitter_url = %s")
            params.append(member.twitter_url)
        if member.email is not None:
            update_fields.append("email = %s")
            params.append(member.email)
        if member.image_url is not None:
            update_fields.append("image_url = %s")
            params.append(member.image_url)
        if member.sort_order is not None:
            update_fields.append("sort_order = %s")
            params.append(member.sort_order)
        if member.is_active is not None:
            update_fields.append("is_active = %s")
            params.append(member.is_active)
        
        if not update_fields:
            return None
        
        params.append(member_id)
        query = f"UPDATE team_members SET {', '.join(update_fields)} WHERE id = %s RETURNING *"
        
        with conn.cursor() as cur:
            cur.execute(query, params)
            row = cur.fetchone()
            conn.commit()
        
        return_db(conn)
        if row:
            return TeamMember.from_orm(row)
        return None
    except DBConnectionError:
        raise HTTPException(
            status_code=503,
            detail="Database unavailable - cannot update team member"
        )
    except Exception as e:
        raise HTTPException(
            status_code=400,
            detail=f"Error updating team member: {str(e)}"
        )


@app.delete("/api/team-members/{member_id}", status_code=204)
async def delete_team_member(member_id: int):
    """
    Delete a team member (soft delete by setting is_active to False).
    
    **Note**: Returns 503 if database is unavailable (write operations require DB).
    """
    try:
        conn = get_db()
        with conn.cursor() as cur:
            cur.execute("UPDATE team_members SET is_active = FALSE WHERE id = %s", (member_id,))
            if cur.rowcount == 0:
                conn.rollback()
                raise HTTPException(status_code=404, detail="Team member not found")
            conn.commit()
        
        return_db(conn)
        return {"message": "Team member deactivated successfully"}
    except DBConnectionError:
        raise HTTPException(
            status_code=503,
            detail="Database unavailable - cannot delete team member"
        )
    except HTTPException:
        raise
    except Exception as e:
        raise HTTPException(
            status_code=400,
            detail=f"Error deleting team member: {str(e)}"
        )


if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        "main:app",
        host=settings.APP_HOST,
        port=settings.APP_PORT,
        reload=True
    )
