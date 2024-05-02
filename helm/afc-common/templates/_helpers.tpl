{{/*
Copyright (C) 2022 Broadcom. All rights reserved.
The term "Broadcom" refers solely to the Broadcom Inc. corporate affiliate
that owns the software below.
This work is licensed under the OpenAFC Project License, a copy of which is
included with this software program.
*/}}

{{/*
Convert name to RFC1123 (no uppercase, no underscores)
Argument is a name to convert
*/}}
{{- define "afc.rfc1123" -}}
{{- . | kebabcase | replace "_" "-" | trimSuffix "-" | trunc 63 -}}
{{- end }}

{{/*
App name that may be used as manifest name/label (chartname if not overridden by .Values.appNameOverride)
*/}}
{{- define "afc.appName" -}}
{{- default .Chart.Name .Values.appNameOverride | include "afc.rfc1123" }}
{{- end }}

{{/*
Helm release name (empty if unspecified)
*/}}
{{- define "afc.releaseName" -}}
{{- eq .Release.Name "release-name" | ternary "" (.Release.Name | include "afc.rfc1123") -}}
{{- end }}

{{/*
Manifest name made from component name
.component must be defined
*/}}
{{- define "afc.compManifestName" -}}
{{- .component | include "afc.rfc1123" -}}
{{- end }}

{{/*
Manifest name made from configmap name
.configmap must be defined
*/}}
{{- define "afc.configmapManifestName" -}}
{{- .configmap | include "afc.rfc1123" -}}
{{- end }}

{{/*
Manifest name made from secret name
.secret must be defined
*/}}
{{- define "afc.secretManifestName" -}}
{{- .secret | include "afc.rfc1123" }}
{{- end }}

{{/*
Manifest name made from secret store name
.secretStore must be defined
*/}}
{{- define "afc.secretStoreManifestName" -}}
{{- .secretStore | include "afc.rfc1123" }}
{{- end }}

{{/*
Hostname for a component
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.hostName" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- default .component (get $compDef "hostname") -}}
{{- end }}

{{/*
Versioned chart name
*/}}
{{- define "afc.versionedChartName" -}}
{{- printf "%s-%s" .Chart.Name .Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Full image name. Empty if image omitted.
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.fullImageName" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $shortName := get $compDef "imageName" -}}
{{- if $shortName -}}
{{-   $repoKey := default (get $compDef "imageRepositoryKey") (get $compDef "imageRepositoryKeyOverride") | required (cat "No 'imageRepositoryKey[Override]' found in definition of component" .component) -}}
{{-   $repoDef := get .Values.imageRepositories $repoKey | required (cat "Component" .component "refers unknown image repository" $repoKey) -}}
{{-   $tag := default .Chart.AppVersion (get $compDef "imageTag") -}}
{{-   $path := get $repoDef "path" -}}
{{-   if $path }}
{{-     $path = printf "%s/" $path }}
{{-   else -}}
{{-     $path = "" }}
{{-   end -}}
{{-   printf "%s%s:%s" $path $shortName $tag }}
{{- else }}
{{-   "" -}}
{{- end }}
{{- end }}

{{/*
Common labels
.component (key in .Values.components) used if defined
*/}}
{{- define "afc.commonLabels" -}}
helm.sh/chart: {{ include "afc.versionedChartName" . | quote }}
{{ include "afc.selectorLabels" . }}
{{- if .Chart.AppVersion }}
app.kubernetes.io/version: {{ .Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ .Release.Service }}
{{- end }}

{{/*
Common app labels (does not include component)
*/}}
{{- define "afc.commonAppLabels" -}}
{{- $savedComponent := get . "component" -}}
{{- $_ := unset . "component" -}}
{{- include "afc.commonLabels" . }}
{{- if $savedComponent -}}
{{-   $_ := set . "component" $savedComponent -}}
{{- end }}
{{- end }}

{{/*
Selector labels
.component (key in .Values.components) used if defined
*/}}
{{- define "afc.selectorLabels" -}}
app.kubernetes.io/name: {{ include "afc.appName" . }}
{{- $releaseName := include "afc.releaseName" . -}}
{{- if $releaseName }}
app.kubernetes.io/instance: {{ $releaseName }}
{{- end }}
{{- if hasKey . "component" }}
app.kubernetes.io/component: {{ include "afc.compManifestName" . }}
{{- end }}
{{- end }}

{{/*
Initial replica count
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.replicas" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{ get $compDef "initialReplicas" | int }}
{{- end }}

{{/*
IP fields (type, loadBalancerIP) in service manifest
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.serviceIp" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
type: {{ get $compDef "serviceType" }}
{{- if hasKey $compDef "loadBalancerIP" }}
loadBalancerIP: {{ get $compDef "loadBalancerIP" }}
{{- end }}
{{- end }}

{{/*
Container name
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.containerName" -}}
{{- include "afc.compManifestName" . -}}
{{- end }}

{{/*
Image definition (container name, image, imagePullPolicy)
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.containerImage" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
name: {{ include "afc.containerName" . }}
image: {{ include "afc.fullImageName" . | required (cat "Image name undefined for component" .component) | quote }}
imagePullPolicy: {{ get $compDef "imagePullPolicy" }}
{{- end }}

{{/*
Image pull secrets (if any)
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.imagePullSecrets" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $repoKey := default (get $compDef "imageRepositoryKey") (get $compDef "imageRepositoryKeyOverride") | required (cat "No 'imageRepositoryKey[Override]' found in definition of component" .component) -}}
{{- $repoDef := get .Values.imageRepositories $repoKey | required (cat "Component" .component "refers unknown image repository" $repoKey) -}}
{{- $pullSecrets := get $repoDef "pullSecrets" -}}
{{- $functionContext := . -}}
{{- if $pullSecrets }}
imagePullSecrets:
{{-   range $secret := $pullSecrets }}
  - name: {{ include "afc.secretManifestName" (mergeOverwrite $functionContext (dict "secret" $secret))  }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Container ports definition
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.containerPorts" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $portDefs := get $compDef "ports" -}}
{{- if $portDefs }}
{{-   range $portName, $portInfo := $portDefs }}
- name: {{ $portName | include "afc.rfc1123" }}
  containerPort: {{ default (get $portInfo "servicePort") (get $portInfo "containerPort" ) | int }}
  protocol: TCP
{{-   end }}
{{- end }}
{{- end }}

{{/*
Headless service ports definition (no nodePort even if defined)
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.servicePortsHeadless" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $portDefs := get $compDef "ports" -}}
{{- if $portDefs }}
{{-   range $portName, $portInfo := $portDefs }}
- name: {{ $portName | include "afc.rfc1123" }}
  port: {{ get $portInfo "servicePort" | int }}
{{-     if hasKey $portInfo "containerPort" }}
  targetPort: {{ get $portInfo "containerPort" }}
{{-     end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Service ports definition
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.servicePorts" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $portDefs := get $compDef "ports" -}}
{{- if $portDefs }}
{{-   range $portName, $portInfo := $portDefs }}
{{-     if hasKey $portInfo "servicePort" }}
- name: {{ $portName | include "afc.rfc1123" }}
  port: {{ get $portInfo "servicePort" | int }}
{{-       if hasKey $portInfo "containerPort" }}
  targetPort: {{ get $portInfo "containerPort" }}
{{-       end }}
{{-       if and (hasKey $portInfo "nodePort") (eq "NodePort" (get $compDef "serviceType")) }}
  nodePort: {{ get $portInfo "nodePort" | int }}
{{-       end }}
  protocol: TCP
{{-     end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Container resources definition
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.containerResources" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $resources := get $compDef "containerResources" -}}
{{- if $resources }}
{{-   toYaml $resources }}
{{- end }}
{{- end }}

{{/*
Persistent Volume Claim template
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.pvcTemplate" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $pvc := get $compDef "pvc" -}}
{{- if and $pvc (hasKey $pvc "name") -}}
metadata:
  name: {{ get $pvc "name" | include "afc.rfc1123" }}
spec:
  accessModes:
    - {{ get $pvc "accessMode" }}
  resources: {{- toYaml (get $pvc "resources") | nindent 4 }}
{{- end }}
{{- end }}

{{/*
Persistent Volume mount
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.pvcMount" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $pvc := get $compDef "pvc" -}}
{{- if and $pvc (hasKey $pvc "name") -}}
name: {{ get $pvc "name" | include "afc.rfc1123" }}
mountPath: {{ get $pvc "mountPath" }}
{{- end }}
{{- end }}

{{/*
Service account reference
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.serviceAccountRef" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $serviceAcc := get $compDef "serviceAccount" -}}
{{- if $serviceAcc }}
serviceAccountName: {{ $serviceAcc }}
{{- end }}
{{- end }}

{{/*
SecurityContext
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.securityContext" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $scKey := get $compDef "securityContextKey" -}}
{{- if $scKey }}
{{-   $sc := get .Values.securityContexts $scKey | required (cat "No security context found for this securityContextKey:" $scKey) -}}
{{-   if $sc }}
securityContext: {{ toYaml $sc | nindent 2 }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Render environment from ConfigMaps, contained in envConfigmapKeys list of component descriptor
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.envFromConfigMaps" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $configmaps := get $compDef "envConfigmapKeys" -}}
{{- $functionContext := . -}}
{{- if $configmaps }}
{{-   range $configmap := $configmaps }}
- configMapRef:
    name: {{ include "afc.configmapManifestName" (mergeOverwrite $functionContext (dict "configmap" $configmap)) }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Renders entries in .Values.configmap subdictionary as configmap environment entries
.configmap (key in .Values.configmaps) must be defined
*/}}
{{- define "afc.configmapEnvEntries" -}}
{{- $values := get .Values.configmaps .configmap | required (cat "No configmap for this configmap key:" .configmap) -}}
{{- $savedComponent := get . "component" -}}
{{- $functionContext := . -}}
{{- range $name, $value := $values }}
{{-   if hasSuffix "@S" $name }}
{{      trimSuffix "@S" $name -}}: {{ toYaml $value }}
{{-   else if hasSuffix "@I" $name }}
{{      trimSuffix "@I" $name -}}: {{ int $value | toString | toYaml }}
{{-   else if kindIs "string" $value }}
{{-     $skip := false }}
{{-     $empty := false }}
{{-     $hostDefs := regexFindAll "\\{\\{host:.+?\\}\\}" $value -1 -}}
{{-     range $hostDef := $hostDefs }}
{{-       $parts := regexFindAll "[^:\\{\\}]+" $hostDef 2 -}}
{{-       $component := index $parts 1 -}}
{{-       $hostName := include "afc.hostName" (mergeOverwrite $functionContext (dict "component" $component)) -}}
{{-       $value = replace $hostDef $hostName $value -}}
{{-     end }}
{{-     $servicePortDefs := regexFindAll "\\{\\{port:.+?:.+?\\}\\}" $value -1 -}}
{{-     range $servicePortDef := $servicePortDefs }}
{{-       $parts := regexFindAll "[^:\\{\\}]+" $servicePortDef 3 -}}
{{-       $component := index $parts 1 -}}
{{-       $portName := index $parts 2 -}}
{{-       $port := include "afc.servicePort" (mergeOverwrite $functionContext (dict "component" $component "portName" $portName)) -}}
{{-       $value = replace $servicePortDef $port $value -}}
{{-     end }}
{{-     $contPortDefs := regexFindAll "\\{\\{containerPort:.+?:.+?\\}\\}" $value -1 -}}
{{-     range $contPortDef := $contPortDefs }}
{{-       $parts := regexFindAll "[^:\\{\\}]+" $contPortDef 3 -}}
{{-       $component := index $parts 1 -}}
{{-       $portName := index $parts 2 -}}
{{-       $port := include "afc.containerPort" (mergeOverwrite $functionContext (dict "component" $component "portName" $portName)) -}}
{{-       $value = replace $contPortDef $port $value -}}
{{-     end }}
{{-     $staticMountDefs := regexFindAll "\\{\\{staticVolumeMount:.+?:.+?\\}\\}" $value -1 -}}
{{-     range $staticMountDef := $staticMountDefs }}
{{-       $parts := regexFindAll "[^:\\{\\}]+" $staticMountDef -1 -}}
{{-       $component := index $parts 1 -}}
{{-       $mountName := index $parts 2 -}}
{{        $ifAbsent := "required" }}
{{-       if ge (len $parts) 4 }}
{{-         $ifAbsent = index $parts 3 -}}
{{-       end }}
{{-       $compDef := merge (dict) (get $functionContext.Values.components $component | required (cat "No component for this component key:" $component)) $functionContext.Values.components.default -}}
{{-       $mounts := default (dict) (get $compDef "staticVolumeMounts") -}}
{{-       $mount := default (get (get $functionContext.Values.staticVolumes $mountName) "defaultMountPath") (get $mounts $mountName) | required (cat "No mount path defined for mount" $mountName "when used in component" $component) -}}
{{-       if not $mount -}}
{{-         if eq $ifAbsent "optional" -}}
{{-           $skip = true -}}
{{-         else if eq $ifAbsent "nullable" }}
{{-           $empty = true -}}
{{-         else }}
{{-           fail (cat "No volume mount named" $mountName "found in static volumes of component" $component) }}
{{-         end }}
{{-       end }}
{{-       $value = replace $staticMountDef $mount $value -}}
{{-     end }}
{{-     $defaultVolumeMountDefs := regexFindAll "\\{\\{defaultVolumeMount:.+?:.+?\\}\\}" $value -1 -}}
{{-     range $defaultVolumeMountDef := $defaultVolumeMountDefs }}
{{-       $parts := regexFindAll "[^:\\{\\}]+" $defaultVolumeMountDef -1 -}}
{{-       $volume := index $parts 1 -}}
{{        $ifAbsent := "required" }}
{{-       if ge (len $parts) 3 }}
{{-         $ifAbsent = index $parts 2 -}}
{{-       end }}
{{-       $defMountpath := get (default (dict) (get $functionContext.Values.staticVolumes $volume)) "defaultMountPath" -}}
{{-       if not $defMountpath -}}
{{-         if eq $ifAbsent "optional" -}}
{{-           $skip = true -}}
{{-         else if eq $ifAbsent "nullable" }}
{{-           $empty = true -}}
{{-         else }}
{{-           fail (cat "No default mount path defined for volume" $volume) }}
{{-         end }}
{{-       end }}
{{-       $value = replace $defaultVolumeMountDef $defMountpath $value -}}
{{-     end }}
{{-     $secretPropertyDefs := regexFindAll "\\{\\{secretProperty:.+?:.+?\\}\\}" $value -1 -}}
{{-     range $secretPropertyDef := $secretPropertyDefs }}
{{-       $parts := regexFindAll "[^:\\{\\}]+" $secretPropertyDef -1 -}}
{{-       $extSecret := index $parts 1 -}}
{{        $ifAbsent := "required" }}
{{-       if ge (len $parts) 3 }}
{{-         $ifAbsent = index $parts 2 -}}
{{-       end }}
{{-       $extSecretDef := merge (dict) (default (dict) (get (default (dict) $functionContext.Values.externalSecrets) $extSecret)) (default (dict) (get (default (dict) $functionContext.Values.externalSecrets) "default")) -}}
{{-       $property := get $extSecretDef "property" -}}
{{-       if not $property -}}
{{-         if eq $ifAbsent "optional" -}}
{{-           $skip = true -}}
{{-         else if eq $ifAbsent "nullable" }}
{{-           $empty = true -}}
{{-         else }}
{{-           fail (cat "External secret named" $extSecret "not found or has empty 'key' property") }}
{{-         end }}
{{-       end }}
{{-       $value = replace $secretPropertyDef $property $value -}}
{{-     end }}
{{-     if not $skip }}
{{        $name -}}: {{ $empty | ternary "" (toYaml $value) }}
{{-     end }}
{{-   else }}
{{      $name -}}: {{ $value | toString | toYaml }}
{{-   end }}
{{- end }}
{{- if $savedComponent -}}
{{-   $_ := set . "component" $savedComponent -}}
{{- end }}
{{- end }}

{{/*
Service port number by name
.component (key in .Values.components) must be defined
.portName (key in components' port dictionary) must be defined
*/}}
{{- define "afc.servicePort" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $portDict := get $compDef "ports" | required (cat "Port dictionary not defined for this component:" .component) -}}
{{- $portDef := get $portDict .portName | required (cat "Component" .component "doesn' have port named" .portName) -}}
{{- $servicePort := get $portDef "servicePort" | required (cat "Component" .component "doesn't define service port number for port" .portName) -}}
{{- $servicePort -}}
{{- end -}}

{{/*
Container (target) port number by name
.component (key in .Values.components) must be defined
.portName (key in components' port dictionary) must be defined
*/}}
{{- define "afc.containerPort" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $servicePort := include "afc.servicePort" . -}}
{{- default $servicePort (get (get (get $compDef "ports") .portName) "containerPort") -}}
{{- end -}}

{{/*
Declaration of static volumes (inhabitatnts of .Values.staticVolumes) in a Deployment/StatefulSet
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.staticVolumes" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $volumes := get $compDef "staticVolumeMounts" -}}
{{- $functionContext := . -}}
{{- if $volumes }}
{{-   range $name := keys $volumes }}
{{-     $volumeDef := "volume" | get (get $functionContext.Values.staticVolumes $name | required (cat "Undefined static volume key" $name "used in definition of component" $functionContext.component)) -}}
{{-     $skip := false -}}
{{-     if hasKey $volumeDef "secret" }}
{{-       $secretName := get (get $volumeDef "secret") "secretName" -}}
{{-       if not (hasKey (default (dict) $functionContext.Values.externalSecrets) $secretName) }}
{{-         $skip = true -}}
{{-       end }}
{{-     end }}
{{-     if not $skip }}
- name: {{ $name | include "afc.rfc1123" }} {{ toYaml $volumeDef | nindent 2 }}
{{-     end }}
{{    end }}
{{- end }}
{{- end }}

{{/*
Mount of static volumes (inhabitatnts of .Values.staticVolumes) in a Deployment/StatefulSet
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.staticVolumeMounts" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $volumes := get $compDef "staticVolumeMounts" -}}
{{- $functionContext := . -}}
{{- if $volumes }}
{{-   range $name, $mount := $volumes }}
{{-     $volumeDef := get (get $functionContext.Values.staticVolumes $name | required (cat "Undefined static volume key" $name "used in definition of component" $functionContext.component)) "volume" -}}
{{-     $skip := false -}}
{{-     if hasKey $volumeDef "secret" }}
{{-       $secretName := get (get $volumeDef "secret") "secretName" -}}
{{-       if not (hasKey (default (dict) $functionContext.Values.externalSecrets) $secretName) }}
{{-         $skip = true -}}
{{-       end }}
{{-     end }}
{{-     if not $skip }}
- name: {{ $name | include "afc.rfc1123" }}
  mountPath: {{ $mount | default (get (get $functionContext.Values.staticVolumes $name) "defaultMountPath") | required (cat "Component " $functionContext.component "does not define mount path for volume" $name "that does not have default mount path") }}
{{-     end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Common Deployment annotations
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.commonDeploymentAnnotations" -}}
kubectl.kubernetes.io/default-container: {{ include "afc.containerName" . }}
{{- end }}

{{/*
Common StatefulSet annotations
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.commonStatefulsetAnnotations" -}}
{{ include "afc.commonDeploymentAnnotations" . }}
{{- end }}

{{/*
Declaration of Prometheus metric endpoint for ServiceMonitor resource
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.metricEndpoints" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $metricsDef := get $compDef "metrics" | required (cat "Component" .component "does not have metrics' definition") -}}
- {{ toYaml $metricsDef | nindent 2 }}
{{- end }}

{{/*
Renders HPA min/maxReplicas
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.hpaReplicas" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $hpaDef := get $compDef "hpa" | required (cat "Component" .component "does not have 'hpa' section") -}}
minReplicas: {{ get $hpaDef "minReplicas" | default 1 | int }}
maxReplicas: {{ get $hpaDef "maxReplicas" | required (cat "Component" .component "does not have 'hpa.maxReplicas' value") | int }}
{{- end }}

{{/*
Renders HPA behavior section (if it is defined)
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.hpaBehavior" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $hpaDef := get $compDef "hpa" | required (cat "Component" .component "does not have 'hpa' section") -}}
{{- $behaviorDef := get $hpaDef "behavior" -}}
{{- if $behaviorDef }}
behavior: {{ toYaml $behaviorDef | nindent 2 }}
{{- end }}
{{- end }}

{{/*
Renders HPA metric
.component (key in .Values.components) must be defined
*/}}
{{- define "afc.hpaMetric" -}}
{{- $compDef := merge (dict) (get .Values.components .component | required (cat "No component for this component key:" .component)) .Values.components.default -}}
{{- $hpaDef := get $compDef "hpa" | required (cat "Component" .component "does not have 'hpa' section") -}}
{{- $metricDef := get $hpaDef "metric" | required (cat "Component" .component "does not have 'hpa.metric' section") -}}
- type: {{ camelcase (mustFirst (keys $metricDef)) }}
  {{- toYaml $metricDef | nindent 2 }}
{{- end }}

{{/*
Renders secret store kind
.secretStore (key in .Values.secretStores) must be defined
*/}}
{{- define "afc.extSecretStoreKind" -}}
{{- $extStoreDef := get .Values.secretStores .secretStore | required (cat "Secret store" .secretStore "not defined in .Values.secretStores") -}}
{{- hasKey $extStoreDef "namespace" | ternary "SecretStore" "ClusterSecretStore" -}}
{{- end }}

{{/*
Renders optional secret store namespace
.secretStore (key in .Values.secretStores) must be defined
*/}}
{{- define "afc.extSecretStoreNamespace" -}}
{{- $extStoreDef := get .Values.secretStores .secretStore | required (cat "Secret store" .secretStore "not defined in .Values.secretStores") -}}
{{- $namespace := get $extStoreDef "namespace" -}}
{{- if $namespace }}
namespace: {{ $namespace }}
{{- end }}
{{- end }}

{{/*
Renders secretStoreRef in ExternalSecret
.secret (key in .Values.externalSecrets) must be defined
*/}}
{{- define "afc.extSecretStoreRef" -}}
{{- $extSecretDef := merge (dict) (get .Values.externalSecrets .secret | required (cat "External secret" .extSecret "not found")) .Values.externalSecrets.default -}}
{{- $secretStore := get $extSecretDef "secretStore" | required (cat "secretStore not defined for external secret" .extSecret) -}}
secretStoreRef:
  name: {{ $secretStore }}
  kind: {{ include "afc.extSecretStoreKind" (mergeOverwrite . (dict "secretStore" $secretStore)) }}
{{- end }}

{{/*
Renders optional refreshInterval
Parameter is dictionary that may contain 'refreshInterval' property - in which case it is rendered
*/}}
{{- define "afc.refreshInterval" -}}
{{- if hasKey . "refreshInterval" }}
refreshInterval: {{ get . "refreshInterval" }}
{{- end }}
{{- end }}

{{/*
Renders target in ExternalSecret
.secret (key in .Values.externalSecrets) must be defined
*/}}
{{- define "afc.extSecretTarget" -}}
{{- $extSecretDef := merge (dict) (get .Values.externalSecrets .secret | required (cat "External secret" .extSecret "not found")) .Values.externalSecrets.default -}}
target:
  name: {{ include "afc.secretManifestName" . }}
  creationPolicy: Owner
  deletionPolicy: Delete
{{- end }}

{{/*
Renders data or dataFromin ExternalSecret
.secret (key in .Values.externalSecrets) must be defined
*/}}
{{- define "afc.extSecretData" -}}
{{- $extSecretDef := merge (dict) (get .Values.externalSecrets .secret | required (cat "External secret" .extSecret "not found")) .Values.externalSecrets.default -}}
{{- $property := get $extSecretDef "property" -}}
{{- if $property }}
data:
  - secretKey: {{ $property }}
    remoteRef:
      key: {{ get $extSecretDef "remoteSecretName" | default .secret | include "afc.rfc1123" }}
{{-   if get $extSecretDef "structuredRemote" }}
      property: {{ get $extSecretDef "remoteProperty" | default $property }}
{{-   end }}
{{- else }}
dataFrom:
  - key:  {{ get $extSecretDef "remoteSecretName" | default .secret | include "afc.rfc1123" }}
{{- end }}
{{- end }}
