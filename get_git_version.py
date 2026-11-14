import subprocess

def get_git_info():
    try:
        sha=subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).strip().decode('utf-8')
    except Exception:
        sha="unknown"

    try:
        tag = subprocess.check_output(['git', 'describe', '--tags', '--exact-match']).strip().decode('utf-8')
    except Exception:
        try:
            tag = subprocess.check_output(['git', 'describe', '--tags', '--abbrev=0']).strip().decode('utf-8')
        except Exception:
            tag = None

    if tag:
        version = f"{tag} ({sha})"
    else:
        version = sha

    with open('include/git_version.h', 'w') as f:
        f.write(f'#define GIT_VERSION "{version}"\n')

if __name__ == '__main__':
    get_git_info()