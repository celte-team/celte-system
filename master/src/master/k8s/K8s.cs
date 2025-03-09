using k8s;
using System.Text.Json;
namespace Celte.Master.K8s
{
    public class K8s
    {
        private static string _namespace;
        private static string _deploymentName;
        private int _maxReplicas = 0;
        private int _minReplicas = 1;
        private static IKubernetes _kubernetesClient;

        public K8s(Dictionary<string, object>? yamlObject)
        {
            try
            {
                var config = KubernetesClientConfiguration.InClusterConfig();
                _kubernetesClient = new Kubernetes(config);
                if (_kubernetesClient == null)
                {
                    throw new Exception("Kubernetes client is not initialized.");
                }
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error initializing Kubernetes client: {e.Message}\n{e.StackTrace}");
            }
            try
            {
                _maxReplicas = int.Parse(yamlObject?["maxReplicas"].ToString());
                _minReplicas = int.Parse(yamlObject?["minReplicas"].ToString());
                _deploymentName = yamlObject?["deploymentName"].ToString();
                _namespace = yamlObject?["namespace"].ToString();
                Console.WriteLine($"Kubernetes client initialized in namespace: {_namespace} \nwith containerId : {Environment.GetEnvironmentVariable("HOSTNAME")}");
                Console.WriteLine($"Let's try to create a new SN!");
                SnIncrease();
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error parsing replica values: {e.Message}\n{e.StackTrace}");
            }
        }

        public async Task<bool> IsDeploymentReady()
        {
            try
            {
                var deployment = await _kubernetesClient.AppsV1.ReadNamespacedDeploymentAsync(_deploymentName, _namespace);
                Console.WriteLine($"Deployment {_deploymentName} status: {deployment.Status.ReadyReplicas} OK");
                return deployment.Status.ReadyReplicas > 0;
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error checking deployment readiness: {e.Message}\n{e.StackTrace}");
                return false;
            }
        }

        public async Task<bool> SnIncrease()
        {
            try
            {
                var deployment = await _kubernetesClient.AppsV1.ReadNamespacedDeploymentAsync(_deploymentName, _namespace);
                int currentReplicas = deployment.Spec.Replicas ?? 1;
                if (currentReplicas < _maxReplicas)
                {
                    int newReplicas = Math.Min(currentReplicas + 1, _maxReplicas);
                    deployment.Spec.Replicas = newReplicas;
                    await _kubernetesClient.AppsV1.ReplaceNamespacedDeploymentAsync(deployment, _deploymentName, _namespace);
                    Console.WriteLine($"Deployment {_deploymentName} scaled to {newReplicas} replicas");
                }
                return true;
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error increasing replicas: {e.Message}\n{e.StackTrace}");
                return false;
            }
        }

        public async Task<bool> SnDecrease(string snId)
        {
            try
            {
                var pod = await _kubernetesClient.CoreV1.ReadNamespacedPodAsync(snId, _namespace);

                await _kubernetesClient.CoreV1.DeleteNamespacedPodAsync(snId, _namespace);
                Console.WriteLine($"Successfully deleted pod {snId}");

                var deployment = await _kubernetesClient.AppsV1.ReadNamespacedDeploymentAsync(_deploymentName, _namespace);
                int currentReplicas = deployment.Spec.Replicas ?? 1;

                if (currentReplicas > _minReplicas)
                {
                    deployment.Spec.Replicas = currentReplicas - 1;
                    await _kubernetesClient.AppsV1.ReplaceNamespacedDeploymentAsync(deployment, _deploymentName, _namespace);
                    Console.WriteLine($"Deployment {_deploymentName} updated to {currentReplicas - 1} replicas");
                }
                return true;
            }
            catch (Exception e)
            {
                Console.WriteLine($"Error decreasing SN: {e.Message}\n{e.StackTrace}");
                return false;
            }
        }
    }
}