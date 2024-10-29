import subprocess
import time
import os
import signal
import threading
import atexit

dotnet_process = None
clock_process = None

def run_docker_compose():
    os.chdir('kafka')
    subprocess.run(['docker', 'compose', 'down'])
    subprocess.run(['docker', 'compose', 'up', '-d'])
    os.chdir('..')

def wait_for_services():
    while True:
        logs = subprocess.check_output(['docker', 'compose', 'logs', 'haproxy'], cwd='kafka').decode('utf-8')
        if '3 active and 0 backup servers online' in logs:
            print("All services are up and running.")
            break
        time.sleep(5)

def run_dotnet():
    os.chdir('master')
    process = subprocess.Popen(['dotnet', 'run', '--config', 'configFile.yml'], preexec_fn=os.setsid)
    os.chdir('..')
    return process

def start_clock():
    os.chdir('ClockServer')
    process = subprocess.Popen(['dotnet', 'run'], preexec_fn=os.setsid)
    os.chdir('..')
    return process

def restart_programs():
    global dotnet_process
    global clock_process
    while True:
        input("Press Enter to restart the programs...")
        os.killpg(os.getpgid(dotnet_process.pid), signal.SIGTERM)
        os.killpg(os.getpgid(clock_process.pid), signal.SIGTERM)
        dotnet_process.wait()
        clock_process.wait()
        run_docker_compose()
        wait_for_services()
        dotnet_process = run_dotnet()
        clock_process = start_clock()

def cleanup():
    global dotnet_process
    global clock_process
    if dotnet_process:
        os.killpg(os.getpgid(dotnet_process.pid), signal.SIGTERM)
        dotnet_process.wait()
    if clock_process:
        os.killpg(os.getpgid(clock_process.pid), signal.SIGTERM)
        clock_process.wait()
    os.chdir('kafka')
    subprocess.run(['docker', 'compose', 'down'])
    os.chdir('..')
    print("Cleanup completed. Docker compose down called.")

if __name__ == "__main__":
    atexit.register(cleanup)
    run_docker_compose()
    wait_for_services()
    dotnet_process = run_dotnet()
    clock_process = start_clock()

    restart_thread = threading.Thread(target=restart_programs)
    restart_thread.daemon = True
    restart_thread.start()

    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        pass