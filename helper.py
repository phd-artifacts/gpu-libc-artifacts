import os
import click
import subprocess

# =============================================================================
# Constants and Path Definitions
# =============================================================================

# Path to the Singularity/Apptainer container image
CONTAINER_IMAGE_PATH = os.path.abspath("sifs/ompc-base_latest.sif")

# Directory paths
SCRIPT_DIRECTORY = os.path.dirname(os.path.abspath(__file__))  # Location of this script
REPO_ROOT_DIRECTORY = os.path.abspath(os.path.join(SCRIPT_DIRECTORY, "."))  # Script is at the repo root

# Shell scripts directory
SCRIPTS_DIRECTORY = os.path.join(REPO_ROOT_DIRECTORY, "sh-scripts")
SET_ENV_SCRIPT_PATH = os.path.abspath(os.path.join(SCRIPTS_DIRECTORY, "set_env.sh"))

# LLVM installation path
INSTALL_DIRECTORY = os.path.abspath(
    os.path.join(REPO_ROOT_DIRECTORY, "llvm-infra/llvm-builds/apptainer-Debug")
)

# =============================================================================
# Helper Functions
# =============================================================================

def execute_command(command, working_directory=REPO_ROOT_DIRECTORY):
    """
    Execute a shell command in a specified directory.

    :param command: Shell command to be executed (string).
    :param working_directory: Path where the command will be executed (string).
    """
    click.echo(f"Executing command in {working_directory}:\n  {command}")
    try:
        subprocess.run(command, shell=True, check=True, cwd=working_directory)
    except subprocess.CalledProcessError as error:
        click.secho(f"Error: {error}", fg="red")
        exit(1)

def build_application(clean, run_cmake):
    """
    Build (compile) the application inside the container.

    :param clean: Boolean indicating whether to perform a clean build.
    """
    build_script_path = os.path.join(SCRIPTS_DIRECTORY, "build_llvm.sh")
    run_cmake_option = "--run-cmake" if run_cmake else ""
    clean_option = "--clean" if clean else ""
    command = f"apptainer exec --nv --bind {SCRIPT_DIRECTORY} {CONTAINER_IMAGE_PATH} {build_script_path} {clean_option} {run_cmake_option}"
    execute_command(command)

def run_application():
    """
    Run the compiled application inside the container.
    Sources the environment script before executing the application.
    """
    application_script = "./run.sh"
    # Note: We don't surround the entire bash command in double quotes.
    # That way Apptainer sees 'bash -c ...' as the command, rather than a single path.
    command = (
        f"apptainer exec --nv --bind {SCRIPT_DIRECTORY} {CONTAINER_IMAGE_PATH} "
        f"bash -c 'source {SET_ENV_SCRIPT_PATH} {INSTALL_DIRECTORY} && {application_script}'"
    )
    # The working directory is where `run.sh` is located
    execute_command(command, working_directory="./application/file-open")

def main_cli(build, run, clean, run_cmake):
    """
    Main logic to handle user commands.

    :param build: Boolean indicating if the user wants to build the application.
    :param run: Boolean indicating if the user wants to run the application.
    :param clean: Boolean indicating if a clean build is desired.
    """
    if build:
        build_application(clean, run_cmake)
    if run:
        run_application()

# =============================================================================
# CLI Setup
# =============================================================================

@click.command()
@click.option('--build', '-b', is_flag=True, help="Build the application.")
@click.option('--run', '-r', is_flag=True, help="Run the application.")
@click.option('--clean', '-c', is_flag=True, help="Clean build before compiling.")
@click.option('--run-cmake', '-m', is_flag=True, help="Re-run cmake before build.")
def cli(build, run, clean, run_cmake):
    """Command Line Interface for building and running the application."""
    main_cli(build, run, clean, run_cmake)

if __name__ == "__main__":
    cli()
