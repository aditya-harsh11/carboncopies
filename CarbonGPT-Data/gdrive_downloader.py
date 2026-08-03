import os
import requests
from dotenv import dotenv_values

# --- Load Configuration from .env file ---
config = dotenv_values(".env")

# --- Configuration ---
API_KEY = config.get("GOOGLE_API_KEY")
PUBLIC_FOLDER_ID = config.get("DRIVE_FOLDER_ID")
API_BASE_URL = "https://www.googleapis.com/drive/v3/files"
DOWNLOAD_DIR = "Dataset/ResearchDocuments/downloaded_drive_files"

# --- Validation ---
if not API_KEY or not PUBLIC_FOLDER_ID:
    print("❌ ERROR: Required configuration is missing.")
    if not os.path.exists(".env"):
        print("Please create a .env file with GOOGLE_API_KEY and DRIVE_FOLDER_ID.")
    else:
        print("Please ensure GOOGLE_API_KEY and DRIVE_FOLDER_ID are defined in your .env file.")
    exit(1)

# Set up the download directory
if not os.path.exists(DOWNLOAD_DIR):
    os.makedirs(DOWNLOAD_DIR)

print(f"✅ Configuration Loaded. Files will be saved in: ./{DOWNLOAD_DIR}")

# --- MIME TYPE MAPPING FOR EXPORT ---
# Primary format, with fallback for large files
GOOGLE_MIME_TYPES = {
    'application/vnd.google-apps.document': {
        'export_mime': 'application/pdf', 
        'extension': '.pdf',
        'fallback_mime': 'application/vnd.openxmlformats-officedocument.wordprocessingml.document',
        'fallback_extension': '.docx'
    },
    'application/vnd.google-apps.spreadsheet': {
        'export_mime': 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet', 
        'extension': '.xlsx'
    },
    'application/vnd.google-apps.presentation': {
        'export_mime': 'application/pdf', 
        'extension': '.pdf',
        'fallback_mime': 'application/vnd.openxmlformats-officedocument.presentationml.presentation',
        'fallback_extension': '.pptx'
    },
    'application/vnd.google-apps.drawing': {
        'export_mime': 'image/png', 
        'extension': '.png'
    },
    'application/vnd.google-apps.script': {
        'export_mime': 'application/vnd.google-apps.script+json', 
        'extension': '.json'
    },
}

def resolve_shortcut(file_id, api_key):
    """
    Resolves a Google Drive shortcut to get the target file's ID and MIME type.
    Also verifies that the target file is accessible.
    """
    params = {
        'key': api_key,
        'fields': 'shortcutDetails(targetId, targetMimeType), name',
        'supportsAllDrives': True,
    }
    
    try:
        response = requests.get(f"{API_BASE_URL}/{file_id}", params=params)
        response.raise_for_status()
        data = response.json()
        
        if 'shortcutDetails' in data:
            target_id = data['shortcutDetails'].get('targetId')
            target_mime = data['shortcutDetails'].get('targetMimeType')
            
            # Verify the target file is accessible
            verify_params = {
                'key': api_key,
                'fields': 'id,name,mimeType',
                'supportsAllDrives': True,
            }
            verify_response = requests.get(f"{API_BASE_URL}/{target_id}", params=verify_params)
            
            if verify_response.status_code == 404:
                print(f"     ⚠️  Target file is not accessible (not publicly shared)")
                return None, None
            
            verify_response.raise_for_status()
            
            return target_id, target_mime
        
        return None, None
        
    except Exception as e:
        print(f"  -> ⚠️  Failed to resolve shortcut: {e}")
        return None, None


def list_public_files(folder_id, api_key):
    """Lists files in the specified public Google Drive folder using an API Key."""

    query_string = f"'{folder_id}' in parents and trashed=false"

    params = {
        'q': query_string,
        'key': api_key,
        'fields': 'nextPageToken, files(id, name, mimeType, size)',
        'pageSize': 100,
        'supportsAllDrives': True,
        'includeItemsFromAllDrives': True,
    }

    files_list = []
    page_token = None

    print(f"\nConnecting to public Google Drive folder: {folder_id}")

    while True:
        if page_token:
            params['pageToken'] = page_token

        try:
            response = requests.get(API_BASE_URL, params=params)
            
            if response.status_code != 200:
                print(f"Debug - Response Body: {response.text}")
            
            response.raise_for_status()

            results = response.json()
            
            files_in_response = results.get('files', [])
            print(f"Found {len(files_in_response)} items in this page...")
            
            for item in files_in_response:
                files_list.append(item)

            page_token = results.get('nextPageToken')
            if not page_token:
                break
                
        except requests.exceptions.HTTPError as e:
            print(f"❌ HTTP Error: {e}")
            print(f"Response: {response.text}")
            return []
        except Exception as e:
            print(f"❌ Error: {e}")
            return []

    return files_list


def download_file(file_id, file_name, mime_type, api_key):
    """
    Downloads a single file. Uses the 'export' endpoint for Google native files
    and the 'get' endpoint for standard files.
    """
    
    # 0. Handle shortcuts - resolve to target file
    if mime_type == 'application/vnd.google-apps.shortcut':
        print(f"  -> Resolving shortcut: {file_name}")
        target_id, target_mime = resolve_shortcut(file_id, api_key)
        
        if not target_id or not target_mime:
            print(f"  -> ❌ Could not resolve shortcut: {file_name}")
            return False
        
        # Update to use target file's info
        file_id = target_id
        mime_type = target_mime
        print(f"     Resolved to: {target_mime}")
    
    # 1. Handle Google Drive Folders
    if mime_type == 'application/vnd.google-apps.folder':
        print(f"  -> Skipping folder: {file_name}")
        return False
        
    # 2. Handle Google Native Files (Export Endpoint)
    elif mime_type in GOOGLE_MIME_TYPES:
        info = GOOGLE_MIME_TYPES[mime_type]
        export_mime_type = info['export_mime']
        new_file_name = file_name + info['extension']
        
        download_url = f"{API_BASE_URL}/{file_id}/export?mimeType={export_mime_type}&key={api_key}"
        local_path = os.path.join(DOWNLOAD_DIR, new_file_name)
        
        print(f"  -> Exporting Google Document ({file_name}) to {info['extension']}...")
        
        # Try to download with primary format
        try:
            response = requests.get(download_url)
            
            # If file is too large, try fallback format
            if response.status_code == 403 and 'too large' in response.text.lower():
                if 'fallback_mime' in info:
                    print(f"     ⚠️  File too large for {info['extension']}, trying {info['fallback_extension']}...")
                    export_mime_type = info['fallback_mime']
                    new_file_name = file_name + info['fallback_extension']
                    download_url = f"{API_BASE_URL}/{file_id}/export?mimeType={export_mime_type}&key={api_key}"
                    local_path = os.path.join(DOWNLOAD_DIR, new_file_name)
                    
                    response = requests.get(download_url)
                    
                    # If still too large, try web download method as last resort
                    if response.status_code == 403 and 'too large' in response.text.lower():
                        print(f"     ⚠️  Still too large, trying web download method...")
                        # Use the web interface export URL (works for larger files)
                        web_export_url = f"https://docs.google.com/document/d/{file_id}/export?format=docx"
                        response = requests.get(web_export_url)
                else:
                    # No fallback, try web download directly
                    print(f"     ⚠️  File too large, trying web download method...")
                    web_export_url = f"https://docs.google.com/document/d/{file_id}/export?format=pdf"
                    response = requests.get(web_export_url)
            
            response.raise_for_status()

            with open(local_path, 'wb') as f:
                f.write(response.content)

            print(f"  ✅ Downloaded: {os.path.basename(local_path)}")
            return True

        except requests.exceptions.RequestException as e:
            print(f"  -> ❌ Failed to download/export {file_name} (ID: {file_id}). Error: {e}")
            if hasattr(e, 'response') and e.response is not None:
                print(f"     Response: {e.response.text[:200]}...")
            return False
        
    # 3. Handle Standard Files
    else:
        download_url = f"{API_BASE_URL}/{file_id}?alt=media&key={api_key}&supportsAllDrives=true"
        local_path = os.path.join(DOWNLOAD_DIR, file_name)
        
        print(f"  -> Downloading standard file: {file_name}")

        try:
            response = requests.get(download_url)
            response.raise_for_status()

            with open(local_path, 'wb') as f:
                f.write(response.content)

            print(f"  ✅ Downloaded: {os.path.basename(local_path)}")
            return True

        except requests.exceptions.RequestException as e:
            print(f"  -> ❌ Failed to download/export {file_name} (ID: {file_id}). Error: {e}")
            if hasattr(e, 'response') and e.response is not None:
                print(f"     Response: {e.response.text[:200]}...")
            return False


if __name__ == '__main__':
    print("\n" + "="*60)
    
    # 1. List the files in the public folder
    documents = list_public_files(PUBLIC_FOLDER_ID, API_KEY)

    # Filter out folders
    downloadable_items = [doc for doc in documents if doc.get('mimeType') != 'application/vnd.google-apps.folder']
    
    print(f"\nTotal items found: {len(documents)}")
    print(f"Downloadable items (excluding folders): {len(downloadable_items)}")
    
    if not downloadable_items:
        print('\n❌ No downloadable documents found in the folder.')
    else:
        print(f"\nStarting download to '{DOWNLOAD_DIR}'...\n")
        print("="*60)

        download_count = 0
        for i, doc in enumerate(downloadable_items, 1):
            print(f"\n[{i}/{len(downloadable_items)}] Processing: {doc['name']}")
            if download_file(doc['id'], doc['name'], doc['mimeType'], API_KEY):
                download_count += 1

        print("\n" + "="*60)
        print(f"✅ Finished. Successfully downloaded {download_count} of {len(downloadable_items)} files.")
        
        if download_count < len(downloadable_items):
            failed = len(downloadable_items) - download_count
            print(f"\n⚠️  {failed} file(s) could not be downloaded.")
            print("\nCommon reasons:")
            print("  • Shortcuts pointing to files that aren't publicly shared")
            print("  • Files located in private folders")
            print("\nTo fix:")
            print("  1. Copy the actual files into the public folder (don't use shortcuts)")
            print("  2. OR make sure each target file is also publicly shared")
        
        print("="*60)
