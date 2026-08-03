#!/usr/bin/env python3
"""
Script to convert downloaded Google Drive files to LLM-friendly markdown format.
Uses markitdown for PDF, DOCX, PPTX, XLSX, ODS, and other document conversions.

Requirements:
    pip install -r requirements.txt

Usage: python3 convert_to_llm_format.py
"""

import os
import sys
import time
from pathlib import Path

# --- Configuration ---
DOWNLOAD_DIR = "Dataset/ResearchDocuments/downloaded_drive_files"
OUTPUT_DIR = "Dataset/ResearchDocuments/llm_ready_markdown"

# Supported file extensions
SUPPORTED_EXTENSIONS = ['.pdf', '.docx', '.pptx', '.xlsx', '.ods', '.md', '.txt']


def ensure_output_directory():
    """Create output directory if it doesn't exist."""
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    print(f"✅ Output directory set to: ./{OUTPUT_DIR}")


def get_files_to_convert(download_dir):
    """Get list of files to convert from download directory."""
    files = []
    
    if not os.path.exists(download_dir):
        print(f"⚠️  Download directory '{download_dir}' does not exist.")
        return files
    
    for filename in os.listdir(download_dir):
        filepath = os.path.join(download_dir, filename)
        if os.path.isfile(filepath):
            ext = os.path.splitext(filename)[1].lower()
            if ext in SUPPORTED_EXTENSIONS:
                files.append(filepath)
    
    return files


def convert_to_markdown(filepath):
    """Convert a file to markdown using markitdown.
    
    Supports: PDF, DOCX, PPTX, XLSX, ODS, MD, TXT
    """
    try:
        from markitdown import MarkItDown
        
        md = MarkItDown()
        result = md.convert(filepath)
        return result.text_content
    except ImportError:
        print(f"  ⚠️  markitdown not installed. Run: pip install markitdown[all]")
        return None
    except Exception as e:
        # Extract just the error message (not the full traceback)
        error_msg = str(e).split('\n')[0] if str(e) else "Unknown error"
        print(f"  ⚠️  {error_msg}")
        return None


def convert_docx_to_markdown(docx_path):
    """Fallback conversion for DOCX files using python-docx."""
    try:
        from docx import Document
        
        doc = Document(docx_path)
        md_lines = []
        
        for para in doc.paragraphs:
            line = para.text
            if not line.strip():
                continue
            
            # Handle headings
            if para.style.name.startswith('Heading'):
                try:
                    level = int(para.style.name.split(' ')[-1])
                    prefix = '#' * min(level, 6)
                    line = f"{prefix} {line}"
                except:
                    line = f"## {line}"
            md_lines.append(line)
        
        return '\n\n'.join(md_lines)
    except Exception as e:
        print(f"  ⚠️  DOCX conversion error: {e}")
        return None


def sanitize_filename(filename):
    """Sanitize filename for use as markdown filename."""
    sanitized = filename.replace('/', '_').replace('\\', '_')
    sanitized = sanitized.replace(':', '_').replace('*', '_')
    sanitized = sanitized.replace('?', '_').replace('"', '_')
    sanitized = sanitized.replace('<', '_').replace('>', '_')
    sanitized = sanitized.replace('|', '_')
    return sanitized


def convert_file(filepath):
    """Convert a single file to markdown."""
    filename = os.path.basename(filepath)
    ext = os.path.splitext(filename)[1].lower()
    
    # Create sanitized output filename
    base_name = os.path.splitext(filename)[0]
    sanitized_name = sanitize_filename(base_name)
    output_filename = f"{sanitized_name}.md"
    output_path = os.path.join(OUTPUT_DIR, output_filename)
    
    print(f"\n[{filename}]")
    print(f"  -> Processing...")
    
    # Try markitdown first (works for PDF, PPTX, XLSX, DOCX, ODS, MD, TXT)
    md_content = convert_to_markdown(filepath)
    
    # Fallback for DOCX using python-docx if markitdown fails
    if ext == '.docx' and md_content is None:
        md_content = convert_docx_to_markdown(filepath)
    
    if md_content and md_content.strip():
        # Save to output file
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(md_content)
        
        file_size = os.path.getsize(output_path)
        print(f"  ✅ Saved: {output_filename} ({file_size:,} bytes)")
        return True
    else:
        print(f"  ❌ Failed to convert: {filename}")
        return False


def main():
    """Main function to orchestrate the conversion process."""
    print("=" * 60)
    print("LLM Document Conversion Script")
    print("=" * 60)
    
    # Set up directories
    ensure_output_directory()
    
    # Get files to convert
    files = get_files_to_convert(DOWNLOAD_DIR)
    
    if not files:
        print(f"\n⚠️  No supported files found in {DOWNLOAD_DIR}")
        print(f"Supported extensions: {', '.join(SUPPORTED_EXTENSIONS)}")
        sys.exit(0)
    
    print(f"\n📚 Found {len(files)} file(s) to convert:")
    for i, filepath in enumerate(files, 1):
        filename = os.path.basename(filepath)
        print(f"  [{i}] {filename}")
    
    print("\n" + "=" * 60)
    print("Starting conversion...")
    print("=" * 60)
    
    # Convert all files
    start_time = time.time()
    success_count = 0
    
    for filepath in files:
        if convert_file(filepath):
            success_count += 1
    
    # Summary
    elapsed_time = time.time() - start_time
    print("\n" + "=" * 60)
    print(f"✅ Conversion complete!")
    print(f"   Successfully converted: {success_count}/{len(files)} file(s)")
    print(f"   Time elapsed: {elapsed_time:.2f} seconds")
    print(f"   Output directory: ./{OUTPUT_DIR}")
    print("=" * 60)
    
    # List output files
    if success_count > 0:
        print("\n📄 Generated markdown files:")
        output_files = sorted(
            [f for f in os.listdir(OUTPUT_DIR) if f.endswith('.md')],
            key=lambda x: os.path.getsize(os.path.join(OUTPUT_DIR, x)),
            reverse=True
        )
        for filename in output_files:
            filepath = os.path.join(OUTPUT_DIR, filename)
            size = os.path.getsize(filepath)
            print(f"   └─ {filename} ({size:,} bytes)")


if __name__ == "__main__":
    main()
