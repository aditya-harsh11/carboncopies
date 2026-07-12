"""Database connection management with graceful error handling"""
import psycopg2
from psycopg2 import pool, OperationalError
import os
from typing import Optional


class DBConnectionError(Exception):
    """Custom exception for database connection issues"""
    pass


# Connection pool for better performance
_connection_pool = None


def init_connection_pool():
    """Initialize connection pool"""
    global _connection_pool
    if _connection_pool is None:
        try:
            _connection_pool = pool.ThreadedConnectionPool(
                minconn=1,
                maxconn=10,
                host=os.getenv("DB_HOST", "localhost"),
                port=os.getenv("DB_PORT", "5432"),
                dbname=os.getenv("DB_NAME", "ccf_website"),
                user=os.getenv("DB_USER", "chardapple"),
                password=os.getenv("DB_PASSWORD", "")
            )
            print("Database connection pool initialized successfully")
        except OperationalError as e:
            print(f"Warning: Could not create connection pool: {e}")
            _connection_pool = None


def get_db():
    """
    Get database connection from pool.
    Raises DBConnectionError if connection cannot be established.
    """
    if _connection_pool is None:
        init_connection_pool()
    
    if _connection_pool is None:
        raise DBConnectionError("Database connection pool not initialized")
    
    try:
        conn = _connection_pool.getconn()
        # Test connection
        with conn.cursor() as cur:
            cur.execute("SELECT 1")
        return conn
    except OperationalError as e:
        raise DBConnectionError(f"Database connection failed: {e}")


def return_db(conn):
    """Return connection to pool"""
    if _connection_pool is not None and conn is not None:
        try:
            _connection_pool.putconn(conn)
        except Exception as e:
            print(f"Warning: Could not return connection to pool: {e}")
            if conn:
                conn.close()


def close_all_connections():
    """Close all connections in the pool"""
    global _connection_pool
    if _connection_pool is not None:
        _connection_pool.closeall()
        _connection_pool = None
        print("All database connections closed")
