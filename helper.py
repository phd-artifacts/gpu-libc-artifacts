import os
import sys
import subprocess
from pathlib import Path
import click

# =============================================================================
# Constants and Path Definitions
# =============================================================================

SCRIPT_DIRECTORY = Path(__file__).resolve().parent
REPO_ROOT_DIRECTORY = SCRIPT_DIRECTORY

CONTAINER_IMAGE_PATH = (REPO_ROOT_DIRECTORY / "sifs" / "ompc-base_latest.sif").resolve()
SCRIPTS_DIRECTORY = REPO_ROOT_DIRECTORY / "sh-scripts"
SET_ENV_SCRIPT_PATH = (SCRIPTS_DIRECTORY / "set_env.sh").resolve()
INSTALL_DIRECTORY = (
    REPO_ROOT_DIRECTORY / "llvm-infra" / "llvm-builds" / "apptainer-Debug"
)

# Determine if we are on a CI environment (to skip apptainer).
IS_CI_ENV = os.getenv("IS_CI_ENV", "false").lower() == "true"

# =============================================================================
# Helper Functions
# =============================================================================

def execute_command(
    command_list: list[str], working_directory: Path = REPO_ROOT_DIRECTORY
) -> None:
    click.echo(f"Executing in {working_directory}:\n  {' '.join(command_list)}")
    try:
        subprocess.run(command_list, check=True, cwd=working_directory)
    except subprocess.CalledProcessError as error:
        click.secho(f"Error: {error}", fg="red")
        sys.exit(1)


def apptainer_exec(
    command_list: list[str], working_directory: Path = REPO_ROOT_DIRECTORY
) -> None:
    if not CONTAINER_IMAGE_PATH.is_file():
        click.secho(
            f"Error: Container image not found at {CONTAINER_IMAGE_PATH}", fg="red"
        )
        sys.exit(1)

    base_cmd = [
        "apptainer",
        "exec",
        "--userns",
        "--no-privs",
        "--bind",
        str(SCRIPT_DIRECTORY),
        str(CONTAINER_IMAGE_PATH),
    ]
    full_command = base_cmd + command_list
    execute_command(full_command, working_directory)


def run_in_environment(
    command_list: list[str], working_directory: Path = REPO_ROOT_DIRECTORY
) -> None:
    if IS_CI_ENV:
        execute_command(command_list, working_directory)
    else:
        apptainer_exec(command_list, working_directory)


def build_application(clean: bool = False, run_cmake: bool = False) -> None:
    build_script_path = SCRIPTS_DIRECTORY / "build_llvm.sh"
    command_list = [str(build_script_path)]
    if clean:
        command_list.append("--clean")
    if run_cmake:
        command_list.append("--run-cmake")
    run_in_environment(command_list)


def run_ninja_command() -> None:
    run_in_environment(["bash", "-c", f"cd {INSTALL_DIRECTORY} && ninja"])


def run_file_build_command() -> None:
    run_in_environment([
        "bash",
        "-c",
        f"cd {INSTALL_DIRECTORY} && ninja copy_ompfile_headers libompfile.so",
    ])


def run_application(application_folder: str) -> None:
    application_directory = (REPO_ROOT_DIRECTORY / "application" / application_folder).resolve()
    run_in_environment([
        "bash",
        "-c",
        f"source {SET_ENV_SCRIPT_PATH} {INSTALL_DIRECTORY} && ./run.sh"
    ], working_directory=application_directory)


def print_shell_help() -> None:
    base_shell_cmd = [
        "apptainer",
        "shell",
        "--bind",
        str(SCRIPT_DIRECTORY),
        str(CONTAINER_IMAGE_PATH),
    ]
    env_cmd = f"source {SET_ENV_SCRIPT_PATH} {INSTALL_DIRECTORY}"

    click.echo("To open an Apptainer shell, run:")
    click.echo(f"  {' '.join(base_shell_cmd)}")
    click.echo("\nThen inside that shell, you might run:")
    click.echo(f"  {env_cmd}")
    click.echo("to source the environment.\n")


@click.command(context_settings={"ignore_unknown_options": True, "allow_extra_args": True})
@click.argument("application_folder", required=True)
@click.pass_context
def cli(ctx, application_folder):
    leftover_args = list(ctx.args)

    while leftover_args:
        cmd = leftover_args.pop(0)

        if cmd == "build":
            clean = False
            run_cmake = False
            while leftover_args and leftover_args[0].startswith("--"):
                flag = leftover_args.pop(0)
                if flag == "--clean":
                    clean = True
                elif flag == "--run-cmake":
                    run_cmake = True
                else:
                    click.secho(f"Unknown option: {flag}", fg="red")
                    sys.exit(1)
            build_application(clean, run_cmake)

        elif cmd == "run":
            run_application(application_folder)

        elif cmd == "shell":
            print_shell_help()

        elif cmd == "ninja":
            run_ninja_command()

        elif cmd == "file-build":
            run_file_build_command()

        else:
            click.secho(f"Unknown command: {cmd}", fg="red")
            sys.exit(1)


if __name__ == "__main__":
    cli()
