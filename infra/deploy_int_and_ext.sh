#!/bin/sh

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
        PROJECT_ID="wcc-enterprise-afc-dev"
        ;;
    -stg)
        arg="stg"
        LOAD_BALANCER_IP="10.255.16.24"
        PROJECT_ID="wcc-enterprise-afc-stg"
        ;;
    -prd|-prod)
        arg="prd"
        LOAD_BALANCER_IP="10.255.8.24"
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
helm install keda kedacore/keda --namespace keda --create-namespace

kubectl wait --for=condition=ready pod -l app.kubernetes.io/name=external-secrets-webhook -n external-secrets --timeout=90s

helm install test-internal afc-int/ -f afc-int/values-$arg.yaml

while [ "$(kubectl get service | grep webui | awk '{print $4}')" == "<pending>" ]; do
    echo "Waiting for webui service to get External-IP"
    kubectl get service | grep webui
    sleep 5
done
AFC_WEBUI_IP=$(kubectl get service | grep webui | awk '{print $4}')
AFC_WEBUI_PORT=$(kubectl get service | grep webui | awk '{print $5}' | cut -d':' -f1)

while [ "$(kubectl get service | grep objst | awk '{print $4}')" == "<pending>" ]; do
    echo "Waiting for objst service to get External-IP"
    kubectl get service | grep objst
    sleep 5
done
AFC_OBJST_HOST=$(kubectl get service | grep objst | awk '{print $4}')
AFC_OBJST_PORT=$(kubectl get service | grep objst | awk '{print $5}' | cut -d':' -f1)
AFC_OBJST_HIST_PORT=$(kubectl get service | grep objst | awk '{print $5}' | cut -d"," -f2 | cut -d":" -f1)

while [ "$(kubectl get service | grep msghnd | awk '{print $4}')" == "<pending>" ]; do
    echo "Waiting for msghnd service to get External-IP"
    kubectl get service | grep msghnd
    sleep 5
done
AFC_MSGHND_IP=$(kubectl get service | grep msghnd | awk '{print $4}')
AFC_MSGHND_PORT=$(kubectl get service | grep msghnd | awk '{print $5}' | cut -d':' -f1)

while [ "$(kubectl get service | grep rmq | awk '{print $4}')" == "<pending>" ]; do
    echo "Waiting for rmq service to get External-IP"
    kubectl get service | grep rmq
    sleep 5
done
AFC_RMQ_IP=$(kubectl get service | grep rmq | awk '{print $4}')
AFC_RMQ_PORT=$(kubectl get service | grep rmq | awk '{print $5}' | cut -d':' -f1)

echo "AFC_WEBUI_IP=$AFC_WEBUI_IP"
echo "AFC_WEBUI_PORT=$AFC_WEBUI_PORT"
echo "AFC_MSGHND_IP=$AFC_MSGHND_IP"
echo "AFC_MSGHND_PORT=$AFC_MSGHND_PORT"
echo "AFC_OBJST_HOST=$AFC_OBJST_HOST"
echo "AFC_OBJST_PORT=$AFC_OBJST_PORT"
echo "AFC_OBJST_HIST_PORT=$AFC_OBJST_HIST_PORT"
echo "AFC_RMQ_IP=$AFC_RMQ_IP"
echo "AFC_RMQ_PORT=$AFC_RMQ_PORT"

cat afc-ext/values.yaml.template | \
    sed "s/%AFC_WEBUI_NAME%/$AFC_WEBUI_IP/g" | \
    sed "s/%AFC_WEBUI_PORT%/$AFC_WEBUI_PORT/g" | \
    sed "s/%AFC_MSGHND_NAME%/$AFC_MSGHND_IP/g" | \
    sed "s/%AFC_MSGHND_PORT%/$AFC_MSGHND_PORT/g" | \
    sed "s/%AFC_OBJST_HOST%/$AFC_OBJST_HOST/g" | \
    sed "s/%AFC_OBJST_PORT%/$AFC_OBJST_PORT/g" | \
    sed "s/%AFC_OBJST_HIST_PORT%/$AFC_OBJST_HIST_PORT/g" | \
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
