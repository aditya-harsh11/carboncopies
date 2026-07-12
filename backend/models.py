"""Pydantic models for team members"""
from pydantic import BaseModel, Field
from typing import Optional
from datetime import datetime
from enum import Enum


class Department(str, Enum):
    """Departments for team members"""
    BOARD_MEMBERS = "Board Members"
    TEAM_MEMBERS = "Team Members"
    ADVISORS = "Advisors"
    RESEARCHERS = "Researchers"
    BRAINGENIX = "BrainGenix Department"


class TeamMemberBase(BaseModel):
    """Base model for team member"""
    name: str = Field(..., description="Full name of the team member")
    title: Optional[str] = Field(None, description="Title or role")
    department: Department = Field(..., description="Department/Category")
    description: Optional[str] = Field(None, description="Short description")
    bio: Optional[str] = Field(None, description="Full biography")
    linkedin_url: Optional[str] = Field(None, description="LinkedIn profile URL")
    twitter_url: Optional[str] = Field(None, description="Twitter/X profile URL")
    email: Optional[str] = Field(None, description="Email address")
    image_url: Optional[str] = Field(None, description="Profile image URL")
    sort_order: Optional[int] = Field(0, description="Sort order")


class TeamMemberCreate(TeamMemberBase):
    """Model for creating a new team member"""
    pass


class TeamMemberUpdate(BaseModel):
    """Model for updating a team member"""
    name: Optional[str] = Field(None, description="Full name of the team member")
    title: Optional[str] = Field(None, description="Title or role")
    department: Optional[Department] = Field(None, description="Department/Category")
    description: Optional[str] = Field(None, description="Short description")
    bio: Optional[str] = Field(None, description="Full biography")
    linkedin_url: Optional[str] = Field(None, description="LinkedIn profile URL")
    twitter_url: Optional[str] = Field(None, description="Twitter/X profile URL")
    email: Optional[str] = Field(None, description="Email address")
    image_url: Optional[str] = Field(None, description="Profile image URL")
    sort_order: Optional[int] = Field(None, description="Sort order")
    is_active: Optional[bool] = Field(None, description="Is active")


class TeamMember(TeamMemberBase):
    """Complete team member model with ID and timestamps"""
    id: int = Field(..., description="Database ID")
    is_active: bool = Field(True, description="Is active")
    created_at: datetime = Field(..., description="Creation timestamp")
    updated_at: datetime = Field(..., description="Last update timestamp")
    
    @classmethod
    def from_orm(cls, row):
        """Convert database row to TeamMember"""
        return cls(
            id=row[0],
            name=row[1],
            title=row[2],
            department=Department(row[3]),
            description=row[4],
            bio=row[5],
            linkedin_url=row[6],
            twitter_url=row[7],
            email=row[8],
            image_url=row[9],
            sort_order=row[10],
            is_active=row[11],
            created_at=row[12],
            updated_at=row[13],
        )
    
    class Config:
        from_attributes = True
