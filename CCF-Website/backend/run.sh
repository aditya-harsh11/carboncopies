#!/bin/bash
# Use system Python 3.9 which has the required packages installed
exec /usr/bin/python3 -m uvicorn main:app --host 0.0.0.0 --port 8080 --reload
