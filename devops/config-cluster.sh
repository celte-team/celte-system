kind create cluster --config kind-config.yaml
kubectl apply -f https://github.com/kubernetes-sigs/metrics-server/releases/latest/download/components.yaml
kubectl get nodes
kind load docker-image celte-system:latest
kind load docker-image celte-master:latest


# /////////

kubectl apply -f server-node-deployment.yaml
kubectl apply -f server-node-hpa.yaml
kubectl apply -f master-deployment.yaml

# kubectl delete deployment master