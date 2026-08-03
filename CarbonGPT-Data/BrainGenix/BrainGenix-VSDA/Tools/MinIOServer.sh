#!/bin/bash

# MinIO Server Management Script for BrainGenix VSDA
# This script helps manage the MinIO server for S3-compatible storage

MINIO_BINARY="./minio"
MINIO_DATA_DIR="./minio-data"
MINIO_PORT=9000
MINIO_CONSOLE_PORT=9001

case "$1" in
    start)
        echo "Starting MinIO server..."
        if [ ! -f "$MINIO_BINARY" ]; then
            echo "MinIO binary not found. Downloading..."
            curl -O https://dl.min.io/server/minio/release/linux-amd64/minio
            chmod +x minio
        fi
        
        if [ ! -d "$MINIO_DATA_DIR" ]; then
            mkdir -p "$MINIO_DATA_DIR"
        fi
        
        # Check if MinIO is already running
        if pgrep -f "minio server" > /dev/null; then
            echo "MinIO server is already running"
            exit 0
        fi
        
        # Start MinIO server in background
        $MINIO_BINARY server $MINIO_DATA_DIR --console-address ":$MINIO_CONSOLE_PORT" &
        MINIO_PID=$!
        echo "MinIO server started with PID: $MINIO_PID"
        echo "S3 API: http://localhost:$MINIO_PORT"
        echo "Web Console: http://localhost:$MINIO_CONSOLE_PORT"
        echo "Default credentials: minioadmin / minioadmin"
        ;;
        
    stop)
        echo "Stopping MinIO server..."
        pkill -f "minio server"
        echo "MinIO server stopped"
        ;;
        
    status)
        if pgrep -f "minio server" > /dev/null; then
            echo "MinIO server is running"
            echo "S3 API: http://localhost:$MINIO_PORT"
            echo "Web Console: http://localhost:$MINIO_CONSOLE_PORT"
        else
            echo "MinIO server is not running"
        fi
        ;;
        
    restart)
        $0 stop
        sleep 2
        $0 start
        ;;
        
    clean)
        echo "Cleaning MinIO data directory..."
        $0 stop
        rm -rf "$MINIO_DATA_DIR"
        echo "MinIO data directory cleaned"
        ;;
        
    test)
        echo "Testing MinIO connection..."
        if curl -s http://localhost:$MINIO_PORT/minio/health/live > /dev/null; then
            echo "✅ MinIO server is responding"
        else
            echo "❌ MinIO server is not responding"
            exit 1
        fi
        ;;
        
    *)
        echo "Usage: $0 {start|stop|restart|status|clean|test}"
        echo ""
        echo "Commands:"
        echo "  start   - Start MinIO server"
        echo "  stop    - Stop MinIO server"
        echo "  restart - Restart MinIO server"
        echo "  status  - Check if MinIO server is running"
        echo "  clean   - Stop server and remove all data"
        echo "  test    - Test if MinIO server is responding"
        echo ""
        echo "Default credentials: minioadmin / minioadmin"
        echo "S3 API: http://localhost:$MINIO_PORT"
        echo "Web Console: http://localhost:$MINIO_CONSOLE_PORT"
        exit 1
        ;;
esac
