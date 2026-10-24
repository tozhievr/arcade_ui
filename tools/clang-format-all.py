import subprocess
from pathlib import Path

def main():
    command = ['clang-format', '-i', '--style', 'file']
    for path in Path("src/").glob("**/*.cppm"):
        command.append(f"src/{path.name}")

    for path in Path("src/").glob("**/*.cpp"):
        command.append(f"src/{path.name}")

    result = subprocess.run(command, encoding='utf8')
    if result.stdout:
        print(f"Error format: {result.stdout}")


if __name__ == '__main__':
    main()