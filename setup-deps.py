import os
import sys
import platform
import urllib.request
import zipfile
import shutil
import hashlib

# LanHouse: cópia validada no NOSSO fork (Release deps-v19 = moonlight-stream/moonlight-qt-deps v19),
# não o repositório upstream - regra do projeto: binário de terceiro sob nosso controle, com SHA256.
ORGANIZATION = "brunoerico"
PREBUILT_REPO = "moonlight-qt"
TAG = "deps-v19"
SHA256 = {
    "macos-universal.zip": "39fab7c95f5d513a6eb3bb19c66f1ae66bbf41dfe4118f82fabe3b46cc502d17",
}

def get_platform_config():
    system = platform.system()
    if system == "Darwin":
        return "mac", "macos-universal.zip"
    print(f"Error: Unsupported platform ({system})")
    sys.exit(1)

def download_and_extract():
    subfolder, asset_name = get_platform_config()
    
    target_dir = os.path.join(os.getcwd(), "libs", subfolder)
    url = f"https://github.com/{ORGANIZATION}/{PREBUILT_REPO}/releases/download/{TAG}/{asset_name}"

    if os.path.exists(target_dir):
        print("Cleaning target directory...")
        shutil.rmtree(target_dir)

    os.makedirs(target_dir, exist_ok=True)

    archive_path = os.path.join(target_dir, asset_name)

    print(f"Downloading {asset_name}...")
    try:
        urllib.request.urlretrieve(url, archive_path)
    except Exception as e:
        print(f"Download failed: {e}")
        sys.exit(1)

    with open(archive_path, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()
    if digest != SHA256[asset_name]:
        print(f"Error: SHA256 de {asset_name} não confere: {digest}")
        sys.exit(1)

    print(f"Extracting {asset_name}...")
    with zipfile.ZipFile(archive_path, 'r') as zip_ref:
        zip_ref.extractall(target_dir)

    os.remove(archive_path)
    print(f"Dependencies successfully deployed")

if __name__ == "__main__":
    download_and_extract()