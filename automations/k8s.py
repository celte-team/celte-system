import subprocess
import os
import yaml

def update_k8s_master_config():
    # Get environment variables
    docker_host_ip = os.environ.get('DOCKER_HOST_IP')
    pulsar_brokers = os.environ.get('PULSAR_BROKERS')
    redis_host = os.environ.get('REDIS_HOST')

    if not docker_host_ip or not pulsar_brokers or not redis_host:
        raise ValueError("The environment variables DOCKER_HOST_IP, PULSAR_BROKERS, and REDIS_HOST must be set.")

    # Path to the Kubernetes configuration file
    # Display the current directory
    master_k8s_config_path = 'devops/master-deployment.yaml'

    with open(master_k8s_config_path, 'r') as f:
        config = yaml.safe_load(f)

    containers = config.get('spec', {}).get('template', {}).get('spec', {}).get('containers', [])
    for container in containers:
        if container.get('name') == 'master':
            env_vars = container.get('env', [])
            for env_var in env_vars:
                if env_var['name'] == 'DOCKER_HOST_IP':
                    env_var['value'] = docker_host_ip
                elif env_var['name'] == 'PULSAR_BROKERS':
                    env_var['value'] = pulsar_brokers
                elif env_var['name'] == 'REDIS_HOST':
                    env_var['value'] = redis_host
    with open(master_k8s_config_path, 'w') as f:
        yaml.safe_dump(config, f, default_flow_style=False)

    print(f"Updated {master_k8s_config_path} with new environment variables.")

def is_minikube_running():
    """Check if Minikube is running."""
    try:
        result = subprocess.run(["minikube", "status", "-o", "json"],
                                stdout=subprocess.PIPE,
                                stderr=subprocess.PIPE,
                                text=True)
        if "Running" in result.stdout:
            return True
        return False

    except FileNotFoundError:
        print("Minikube is not installed or not in PATH.")
        return False

def start_minikube():
    """Start Minikube if not running."""
    subprocess.run(["minikube", "start"], check=True)

def deploy_k8s_resources():
    """Deploy Kubernetes resources from the 'devops' folder."""
    update_k8s_master_config()
    devops_folder = os.path.join(os.getcwd(), "devops")
    master_deployment_path = os.path.join(devops_folder, "master-deployment.yaml")
    master_hpa_path = os.path.join(devops_folder, "master-hpa.yaml")

    if not os.path.exists(master_deployment_path) or not os.path.exists(master_hpa_path):
        raise FileNotFoundError("Required YAML files are not present in the 'devops' folder.")

    print("Applying Kubernetes configurations...")
    # if this command crashes, ignore it and continue
    try:
        subprocess.run(["kubectl", "delete", "-f", master_deployment_path], check=True)
        subprocess.run(["kubectl", "delete", "-f", master_hpa_path], check=True)
    except subprocess.CalledProcessError:
        print("Error while creating Kubernetes resources. Continuing...")
        pass

    subprocess.run(["kubectl", "apply", "-f", master_deployment_path], check=True)
    subprocess.run(["kubectl", "apply", "-f", master_hpa_path], check=True)

def k8s_shutdown():
    # Shutdown the Kubernetes cluster
    if is_minikube_running():
        subprocess.run(["minikube", "stop"], check=True)
        print("Minikube stopped.")
    else:
        print("Minikube is not running.")

def k8s_launch():
    # Launch a new Kubernetes cluster
    if not is_minikube_running():
        # if Minikube is not running then start it
        start_minikube()
    else:
        print("Minikube is already running.")

    deploy_k8s_resources()
    print("you should run k9s -c pod;)")

if __name__ == "__main__":
    k8s_launch()