#!/bin/sh

INT_INGRESS_PORT="80"

# Check if an argument was provided
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 [-dev|-stg|-prd|-prod]"
    exit 1
fi

# Argument handling
case $1 in
    -dev)
        arg="dev"
        LOAD_BALANCER_IP="10.255.0.24"
        INT_INGRESS_IP="10.255.128.213"
        PROJECT_ID="wcc-enterprise-afc-dev"
        ;;
    -stg)
        arg="stg"
        LOAD_BALANCER_IP="10.255.16.24"
        INT_INGRESS_IP="10.255.144.213"
        PROJECT_ID="wcc-enterprise-afc-stg"
        ;;
    -prd|-prod)
        arg="prd"
        LOAD_BALANCER_IP="10.255.8.24"
        INT_INGRESS_IP="10.255.136.213"
        PROJECT_ID="wcc-enterprise-afc-prd"
        ;;
    *)
        echo "Invalid argument: $1"
        echo "Usage: $0 [-dev|-stg|-prd]"
        exit 1
        ;;
esac

kubectl config use-context $(kubectl config get-contexts | grep 'int' | awk '{print $2}')
kubectl get all

helm repo add bitnami https://charts.bitnami.com/bitnami
helm repo add kedacore https://kedacore.github.io/charts
helm repo add external-secrets https://charts.external-secrets.io
helm repo update
helm install external-secrets \
   external-secrets/external-secrets \
    -n external-secrets \
    --create-namespace \
    --set installCRDs=true

kubectl wait --for=condition=ready pod -l app.kubernetes.io/name=external-secrets-webhook -n external-secrets

helm install keda kedacore/keda --namespace keda --create-namespace

helm install ingress-nginx ingress-nginx \
    --repo https://kubernetes.github.io/ingress-nginx \
    --namespace ingress-nginx \
    --create-namespace \
    --set controller.service.annotations."networking\.gke\.io/internal-load-balancer-allow-global-access"="true" \
    --set contorller.service.internal.enabled="true" \
    --set controller.service.internal.annotations."cloud\.google\.com/load-balancer-type"="Internal" \
    --set controller.service.annotations."networking\.gke\.io/load-balancer-type"="Internal" \
    --set controller.service.loadBalancerIP=$INT_INGRESS_IP

kubectl wait --namespace ingress-nginx \
  --for=condition=ready pod \
  --selector=app.kubernetes.io/component=controller \
  --timeout=120s

kubectl rollout status deployment/keda-admission-webhooks --namespace keda
kubectl rollout status deployment/keda-operator --namespace keda
kubectl rollout status deployment/keda-operator-metrics-apiserver --namespace keda

kubectl delete -A ValidatingWebhookConfiguration ingress-nginx-admission

#helm install test-internal afc-int/ -f afc-int/values-$arg.yaml

if ! helm install test-internal afc-int/ -f afc-int/values-$arg.yaml; then
    echo "Helm install failed, exiting script."
    exit 1
fi

while [ "$(kubectl get service | grep rmq-lb | awk '{print $4}')" == "<pending>" ]; do
    echo "Waiting for rmq-lb service to get External-IP"
    kubectl get service | grep rmq-lb
    sleep 5
done
AFC_RMQ_IP=$(kubectl get service | grep rmq-lb | awk '{print $4}')
AFC_RMQ_PORT=$(kubectl get service | grep rmq-lb | awk '{print $5}' | cut -d':' -f1)

echo "AFC_RMQ_IP=$AFC_RMQ_IP"
echo "AFC_RMQ_PORT=$AFC_RMQ_PORT"

cat afc-ext/values.yaml.template | \
    sed "s/%AFC_INT_INGRESS_NGINX_IP%/$INT_INGRESS_IP/g" | \
    sed "s/%AFC_INT_INGRESS_NGINX_PORT%/$INT_INGRESS_PORT/g" | \
    sed "s/%AFC_RMQ_NAME%/$AFC_RMQ_IP/g" | \
    sed "s/%AFC_RMQ_PORT%/$AFC_RMQ_PORT/g" | \
    sed "s/%SECRET_STORE_PROJ_ID%/$PROJECT_ID/g" | \
    sed "s/%LOAD_BALANCER_IP%/$LOAD_BALANCER_IP/g" > afc-ext/values.yaml

kubectl config use-context $(kubectl config get-contexts | grep 'ext' | awk '{print $2}')
kubectl get all

helm repo add external-secrets https://charts.external-secrets.io
helm repo update
helm install external-secrets \
   external-secrets/external-secrets \
    -n external-secrets \
    --create-namespace \
    --set installCRDs=true

kubectl wait --for=condition=ready pod -l app.kubernetes.io/name=external-secrets-webhook -n external-secrets --timeout=90s

helm install test-external afc-ext/ -f afc-ext/values.yaml
