#!/bin/sh

kubectl config use-context $(kubectl config get-contexts | grep 'int' | awk '{print $2}')
kubectl get all

helm install test-internal afc-int/ -f afc-int/values.yaml

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
    sed "s/%AFC_RMQ_PORT%/$AFC_RMQ_PORT/g" > afc-ext/values.yaml

kubectl config use-context $(kubectl config get-contexts | grep 'ext' | awk '{print $2}')
kubectl get all

helm install test-external afc-ext/ -f afc-ext/values.yaml
