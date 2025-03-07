using k8s;

namespace Celte.Master.K8s
{
    public class K8s
    {
        private static readonly string Namespace = "celte";
        private static readonly string DeploymentName = "server-node";
        private static readonly int MaxReplicas = 100;
        private static IKubernetes _kubernetesClient;

        public K8s()
        {
            var config = KubernetesClientConfiguration.InClusterConfig();
            _kubernetesClient = new Kubernetes(config);
            Console.WriteLine($"Kubernetes client initialized in namespace: {Namespace}");
        }

        public static async Task<K8s> CreateAsync()
        {
            var config = KubernetesClientConfiguration.InClusterConfig();
            var kubernetesClient = new Kubernetes(config);
            Console.WriteLine($"Kubernetes client initialized 2 in namespace: {Namespace}");
            return new K8s();
        }

        public async Task<bool> IsDeploymentReady()
        {
            try
            {
                var deployment = await _kubernetesClient.AppsV1.ReadNamespacedDeploymentAsync(DeploymentName, Namespace);
                Console.WriteLine($"Deployment {DeploymentName} status: {deployment.Status.ReadyReplicas} OK");
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
                var deployment = await _kubernetesClient.AppsV1.ReadNamespacedDeploymentAsync(DeploymentName, Namespace);
                int currentReplicas = deployment.Spec.Replicas ?? 1;
                if (currentReplicas < MaxReplicas)
                {
                    int newReplicas = Math.Min(currentReplicas + 1, MaxReplicas);
                    deployment.Spec.Replicas = newReplicas;
                    await _kubernetesClient.AppsV1.ReplaceNamespacedDeploymentAsync(deployment, DeploymentName, Namespace);
                    Console.WriteLine($"Deployment {DeploymentName} scaled to {newReplicas} replicas");
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
                var pod = await _kubernetesClient.CoreV1.ReadNamespacedPodAsync(snId, Namespace);

                await _kubernetesClient.CoreV1.DeleteNamespacedPodAsync(snId, Namespace);
                Console.WriteLine($"Successfully deleted pod {snId}");

                var deployment = await _kubernetesClient.AppsV1.ReadNamespacedDeploymentAsync(DeploymentName, Namespace);
                int currentReplicas = deployment.Spec.Replicas ?? 1;

                if (currentReplicas > 1) // Keep at least 1 SN
                {
                    deployment.Spec.Replicas = currentReplicas - 1;
                    await _kubernetesClient.AppsV1.ReplaceNamespacedDeploymentAsync(deployment, DeploymentName, Namespace);
                    Console.WriteLine($"Deployment {DeploymentName} updated to {currentReplicas - 1} replicas");
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