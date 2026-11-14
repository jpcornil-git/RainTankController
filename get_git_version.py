import subprocess

def get_git_sha():
    try:
        sha=subprocess.check_output(['git', 'rev-parse', '--short', 'HEAD']).strip().decode('utf-8')
    except Exception:
        sha="unknown"

    with open('include/git_version.h', 'w') as f:
        f.write(f'#define GIT_VERSION "{sha}"\n')

if __name__ == '__main__':
    get_git_sha()