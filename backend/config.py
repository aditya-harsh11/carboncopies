"""Configuration settings"""
import os
from pydantic_settings import BaseSettings


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
    
    # CORS
    CORS_ORIGINS: list[str] = os.getenv("CORS_ORIGINS", "*").split(",")


settings = Settings()
