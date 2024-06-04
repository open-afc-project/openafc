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
App name that may be used as manifest name/label (chartname if not overridden by $.Values.appNameOverride)
*/}}
{{- define "afc.appName" -}}
{{- default $.Chart.Name $.Values.appNameOverride | include "afc.rfc1123" }}
{{- end }}

{{/*
Helm release name (empty if unspecified)
*/}}
{{- define "afc.releaseName" -}}
{{- eq $.Release.Name "release-name" | ternary "" ($.Release.Name | include "afc.rfc1123") -}}
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
.component (key in $.Values.components) must be defined
.Values must be defined
*/}}
{{- define "afc.hostName" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- default .component (get (default (dict) (get $compDef "service")) "hostname") -}}
{{- end }}

{{/*
Versioned chart name
*/}}
{{- define "afc.versionedChartName" -}}
{{- printf "%s-%s" $.Chart.Name $.Chart.Version | replace "+" "_" | trunc 63 | trimSuffix "-" }}
{{- end }}

{{/*
Secret mount name
Argument is a secret name
*/}}
{{- define "afc.secretMountName" -}}
{{- printf "%s-secret" (include "afc.rfc1123" .) }}
{{- end }}

{{/*
Full image name. Empty if image omitted.
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.fullImageName" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $shortName := get $compDef "imageName" -}}
{{- if $shortName -}}
{{-   $repoKey := default (get $compDef "imageRepositoryKey") (get $compDef "imageRepositoryKeyOverride") | required (cat "No 'imageRepositoryKey[Override]' found in definition of component" .component) -}}
{{-   $repoDef := get $.Values.imageRepositories $repoKey | required (cat "Component" .component "refers unknown image repository" $repoKey) -}}
{{-   $tag := default $.Chart.AppVersion (get $compDef "imageTag") -}}
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
.component (key in $.Values.components) used if defined
*/}}
{{- define "afc.commonLabels" -}}
helm.sh/chart: {{ include "afc.versionedChartName" . | quote }}
{{ include "afc.selectorLabels" . }}
{{- if $.Chart.AppVersion }}
app.kubernetes.io/version: {{ $.Chart.AppVersion | quote }}
{{- end }}
app.kubernetes.io/managed-by: {{ $.Release.Service }}
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
.component (key in $.Values.components) used if defined
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
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.replicas" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{ get $compDef "initialReplicas" | int }}
{{- end }}

{{/*
IP fields (type, loadBalancerIP) in service manifest
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.serviceIp" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $serviceDef := default (dict) (get $compDef "service") -}}
{{- $serviceType := get $serviceDef "type" -}}
{{- $ip := "" -}}
{{- $ipKey := get $serviceDef "loadBalancerIpKey" -}}
{{- if $ipKey -}}
{{-   if not (hasKey $.Values.externalIps $ipKey) -}}
{{-     fail (cat "Service of component" .component "uses undefined loadBalancerIpKey" $ipKey) -}}
{{-   end -}}
{{-   $ip = get $.Values.externalIps $ipKey -}}
{{- end -}}
{{- if $serviceType }}
type: {{ $serviceType }}
{{- end }}
{{- if $ip }}
{{-   if ne $serviceType "LoadBalancer" -}}
{{-     fail (cat "Component" .component "defines loadBalancerIp for non-LoadBalancer service") }}
{{-   end }}
loadBalancerIP: {{ $ip }}
{{- end -}}
{{- end }}

{{/*
Container name
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.containerName" -}}
{{- include "afc.compManifestName" . -}}
{{- end }}

{{/*
Image definition (container name, image, imagePullPolicy)
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.containerImage" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
name: {{ include "afc.containerName" . }}
image: {{ include "afc.fullImageName" . | required (cat "Image name undefined for component" .component) | quote }}
imagePullPolicy: {{ get $compDef "imagePullPolicy" }}
{{- end }}

{{/*
Image pull secrets (if any)
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.imagePullSecrets" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $repoKey := default (get $compDef "imageRepositoryKey") (get $compDef "imageRepositoryKeyOverride") | required (cat "No 'imageRepositoryKey[Override]' found in definition of component" .component) -}}
{{- $repoDef := get $.Values.imageRepositories $repoKey | required (cat "Component" .component "refers unknown image repository" $repoKey) -}}
{{- $pullSecrets := get $repoDef "pullSecrets" -}}
{{- if $pullSecrets }}
imagePullSecrets:
{{-   range $secret := $pullSecrets }}
  - name: {{ include "afc.secretManifestName" (dict "secret" $secret)  }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Container ports definition
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.containerPorts" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
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
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.servicePortsHeadless" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $portDefs := get $compDef "ports" -}}
{{- if $portDefs }}
{{-   range $portName, $portInfo := $portDefs }}
- name: {{ $portName | include "afc.rfc1123" }}
  port: {{ get $portInfo "servicePort" | int }}
{{-     if hasKey $portInfo "containerPort" }}
  targetPort: {{ get $portInfo "containerPort" }}
{{-     end }}
{{-     if hasKey $portInfo "appProtocol" }}
  appProtocol: {{ get $portInfo "appProtocol" }}
{{-     end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Service ports definition
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.servicePorts" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $serviceDef := default (dict) (get $compDef "service") -}}
{{- $portDefs := get $compDef "ports" -}}
{{- if $portDefs }}
{{-   range $portName, $portInfo := $portDefs }}
{{-     if hasKey $portInfo "servicePort" }}
- name: {{ $portName | include "afc.rfc1123" }}
  port: {{ get $portInfo "servicePort" | int }}
{{-       if hasKey $portInfo "containerPort" }}
  targetPort: {{ get $portInfo "containerPort" }}
{{-       end }}
{{-       if and (hasKey $portInfo "nodePort") (eq "NodePort" (get $serviceDef "type")) }}
  nodePort: {{ get $portInfo "nodePort" | int }}
{{-       end }}
{{-       if hasKey $portInfo "appProtocol" }}
  appProtocol: {{ get $portInfo "appProtocol" }}
{{-       end }}
  protocol: TCP
{{-     end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Service annotations
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.serviceAnnotations" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $serviceDef := default (dict) (get $compDef "service") -}}
{{- $annotations := get $serviceDef "annotations" -}}
{{- if $annotations }}
annotations: {{- toYaml $annotations | nindent 2 }}
{{- end }}
{{- end }}

{{/*
Service selector
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.serviceSelector" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $serviceDef := default (dict) (get $compDef "service") -}}
{{- if get $serviceDef "selector" }}
{{-   toYaml (get $serviceDef "selector") }}
{{- else }}
{{-   include "afc.selectorLabels" . }}
{{- end }}
{{- end }}

{{/*
Container resources definition
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.containerResources" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $resources := get $compDef "containerResources" -}}
{{- if $resources }}
{{-   toYaml $resources }}
{{- end }}
{{- end }}

{{/*
Persistent Volume Claim template
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.pvcTemplate" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
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
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.pvcMount" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $pvc := get $compDef "pvc" -}}
{{- if and $pvc (hasKey $pvc "name") -}}
name: {{ get $pvc "name" | include "afc.rfc1123" }}
mountPath: {{ get $pvc "mountPath" }}
{{- end }}
{{- end }}

{{/*
Service account reference
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.serviceAccountRef" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $serviceAcc := get $compDef "serviceAccount" -}}
{{- if $serviceAcc }}
serviceAccountName: {{ $serviceAcc }}
{{- end }}
{{- end }}

{{/*
SecurityContext
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.securityContext" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $scKey := get $compDef "securityContextKey" -}}
{{- if $scKey }}
{{-   $sc := get $.Values.securityContexts $scKey | required (cat "No security context found for this securityContextKey:" $scKey) -}}
{{-   if $sc }}
securityContext: {{ toYaml $sc | nindent 2 }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Render environment from ConfigMaps, contained in envConfigmapKeys list of component descriptor
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.envFromConfigMaps" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $configmaps := get $compDef "envConfigmapKeys" -}}
{{- if $configmaps }}
{{-   range $configmap := $configmaps }}
- configMapRef:
    name: {{ include "afc.configmapManifestName" (dict "configmap" $configmap) }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Mount path of given secret
.secret (key in $.Values.externalSecrets) must be defined
*/}}
{{- define "afc.secretMountPath" -}}
{{- $extSecretDef := merge (dict) (get $.Values.externalSecrets .secret | required (cat "External secret" .secret "not found")) $.Values.externalSecrets.default -}}
{{- hasKey $extSecretDef "mountPath" | ternary (get $extSecretDef "mountPath") (printf "%s%s" (get $extSecretDef "mountPathPrefix" | required (cat "External secret" .secret "has neither 'mountPath' nor 'mountPathPrefix' property")) .secret) -}}
{{- end -}}

{{/*
Renders configmap entry or environment variable value
.value (Value to render) must be defined
.Values (top level values dictionary) must be defined
Parameter is configmap entry or environment variable as specified in values.yanl (i.e. may contain {{...}} expressions - see values.yaml, comment to configmap section for supported expressons)
Returns value or 'SKIP_SKIP_SKIP_SKIP' to skip
*/}}
{{- define "afc.envValue" -}}
{{- $value := .value }}
{{- $skip := false }}
{{- $empty := false }}
{{- $hostDefs := regexFindAll "\\{\\{host:.+?\\}\\}" $value -1 -}}
{{- range $hostDef := $hostDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $hostDef 2 -}}
{{-   $component := index $parts 1 -}}
{{-   $hostName := include "afc.hostName" (dict "component" $component "Values" $.Values) -}}
{{-   $value = replace $hostDef $hostName $value -}}
{{- end }}
{{- $servicePortDefs := regexFindAll "\\{\\{port:.+?:.+?\\}\\}" $value -1 -}}
{{- range $servicePortDef := $servicePortDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $servicePortDef 3 -}}
{{-   $component := index $parts 1 -}}
{{-   $portName := index $parts 2 -}}
{{-   $port := include "afc.servicePort" (dict "component" $component "portName" $portName "Values" $.Values) -}}
{{-   $value = replace $servicePortDef $port $value -}}
{{- end }}
{{- $contPortDefs := regexFindAll "\\{\\{containerPort:.+?:.+?\\}\\}" $value -1 -}}
{{- range $contPortDef := $contPortDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $contPortDef 3 -}}
{{-   $component := index $parts 1 -}}
{{-   $portName := index $parts 2 -}}
{{-   $port := include "afc.containerPort" (dict "component" $component "portName" $portName "Values" $.Values) -}}
{{-   $value = replace $contPortDef $port $value -}}
{{- end }}
{{- $staticMountDefs := regexFindAll "\\{\\{staticVolumeMount:.+?:.+?\\}\\}" $value -1 -}}
{{- range $staticMountDef := $staticMountDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $staticMountDef -1 -}}
{{-   $component := index $parts 1 -}}
{{-   $mountName := index $parts 2 -}}
{{    $ifAbsent := "required" }}
{{-   if ge (len $parts) 4 }}
{{-     $ifAbsent = index $parts 3 -}}
{{-   end }}
{{-   $compDef := merge (dict) (get $.Values.components $component | required (cat "No component for this component key:" $component)) $.Values.components.default -}}
{{-   $mounts := default (dict) (get $compDef "staticVolumeMounts") -}}
{{-   $mount := get $mounts $mountName | required (cat "No mount path defined for mount" $mountName "when used in component" $component) -}}
{{-   if not $mount -}}
{{-     if eq $ifAbsent "optional" -}}
{{-       $skip = true -}}
{{-     else if eq $ifAbsent "nullable" }}
{{-       $empty = true -}}
{{-     else }}
{{-       fail (cat "No volume mount named" $mountName "found in static volumes of component" $component) }}
{{-     end }}
{{-   end }}
{{-   $value = replace $staticMountDef $mount $value -}}
{{- end }}
{{- $secretFileDefs := regexFindAll "\\{\\{secretFile:.+?\\}\\}" $value -1 -}}
{{- range $secretFileDef := $secretFileDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $secretFileDef -1 -}}
{{-   $extSecret := index $parts 1 -}}
{{    $ifAbsent := "required" }}
{{-   if ge (len $parts) 3 }}
{{-     $ifAbsent = index $parts 2 -}}
{{-   end }}
{{-   $extSecretDef := merge (dict) (default (dict) (get (default (dict) $.Values.externalSecrets) $extSecret)) (default (dict) (get (default (dict) $.Values.externalSecrets) "default")) -}}
{{-   $property := get $extSecretDef "property" -}}
{{-   $mountPath := "" -}}
{{-   if $property }}
{{-     $mountPath = include "afc.secretMountPath" (dict "secret" $extSecret "Values" $.Values) }}
{{-   else }}
{{-     if eq $ifAbsent "optional" -}}
{{-       $skip = true -}}
{{-     else if eq $ifAbsent "nullable" }}
{{-       $empty = true -}}
{{-     else }}
{{-       fail (cat "External secret" $extSecret "must be defined and have 'property' and 'mountPath' subkeys") }}
{{-     end }}
{{-   end }}
{{-   $value = replace $secretFileDef (printf "%s/%s" $mountPath $property) $value -}}
{{- end }}
{{- $secretPropertyDefs := regexFindAll "\\{\\{secretProperty:.+?\\}\\}" $value -1 -}}
{{- range $secretPropertyDef := $secretPropertyDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $secretPropertyDef -1 -}}
{{-   $extSecret := index $parts 1 -}}
{{    $ifAbsent := "required" }}
{{-   if ge (len $parts) 3 }}
{{-     $ifAbsent = index $parts 2 -}}
{{-   end }}
{{-   $extSecretDef := merge (dict) (default (dict) (get (default (dict) $.Values.externalSecrets) $extSecret)) (default (dict) (get (default (dict) $.Values.externalSecrets) "default")) -}}
{{-   $property := get $extSecretDef "property" -}}
{{-   if not $property -}}
{{-     if eq $ifAbsent "optional" -}}
{{-       $skip = true -}}
{{-     else if eq $ifAbsent "nullable" }}
{{-       $empty = true -}}
{{-     else }}
{{-       fail (cat "External secret named" $extSecret "not found or has empty 'key' property") }}
{{-     end }}
{{-   end }}
{{-   $value = replace $secretPropertyDef $property $value -}}
{{- end }}
{{- $ipDefs := regexFindAll "\\{\\{ip:.+?\\}\\}" $value -1 -}}
{{- range $ipDef := $ipDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $ipDef -1 -}}
{{-   $ipKey := index $parts 1 -}}
{{    $ifAbsent := "required" }}
{{-   if ge (len $parts) 3 }}
{{-     $ifAbsent = index $parts 2 -}}
{{-   end }}
{{-   if not (hasKey $.Values.externalIps $ipKey) }}
{{-     fail (cat "External IP key" $ipKey "not found in 'externalIps'") }}
{{-   end }}
{{-   $ip := get $.Values.externalIps $ipKey }}
{{-   if not $ip }}
{{-     if eq $ifAbsent "optional" -}}
{{-       $skip = true -}}
{{-     else if eq $ifAbsent "nullable" }}
{{-       $empty = true -}}
{{-     else }}
{{-       fail (cat "External IP" $ipKey "is not defined") }}
{{-     end }}
{{-   end }}
{{-   $value = replace $ipDef $ip $value -}}
{{- end }}
{{- $sameAsDefs := regexFindAll "\\{\\{sameAs:.+?\\}\\}" $value -1 -}}
{{- range $sameAsDef := $sameAsDefs }}
{{-   $parts := regexFindAll "[^:\\{\\}]+" $sameAsDef -1 -}}
{{-   $configmapKey := index $parts 1 -}}
{{-   $valueKey := index $parts 2 -}}
{{    $ifAbsent := "required" }}
{{-   if ge (len $parts) 4 }}
{{-     $ifAbsent = index $parts 3 -}}
{{-   end }}
{{-   $configmapDef := get $.Values.configmaps $configmapKey | required (cat "Configmap" $configmapKey "used in rendering of " $sameAsDef "not found" ) -}}
{{-   if not (hasKey $configmapDef $valueKey) }}
{{-     if eq $ifAbsent "optional" }}
{{-       $skip = true -}}
{{-     else if eq $ifAbsent "nullable" }}
{{-       $empty = true -}}
{{-     else }}
{{-       fail (cat "Value" $valueKey "not found in configmap" $configmapKey "while rendering" $sameAsDef) }}
{{-     end }}
{{-   end }}
{{-   $v := include "afc.envValue" (dict "value" (get $configmapDef $valueKey) "Values" $.Values) -}}
{{-   if eq $v "SKIP_SKIP_SKIP_SKIP" }}
{{-     $skip = true -}}
{{-   end }}
{{-   $value = replace $sameAsDef $v $value -}}
{{- end }}
{{- $skip | ternary "SKIP_SKIP_SKIP_SKIP" ($empty | ternary "" $value) }}
{{- end }}

{{/*
Renders entries in $.Values.configmap subdictionary as configmap environment entries
.configmap (key in $.Values.configmaps) must be defined
*/}}
{{- define "afc.configmapEnvEntries" -}}
{{- $values := get $.Values.configmaps .configmap | required (cat "No configmap for this configmap key:" .configmap) -}}
{{- range $name, $value := $values }}
{{-   if hasSuffix "@S" $name }}
{{      trimSuffix "@S" $name -}}: {{ toYaml $value }}
{{-   else if hasSuffix "@I" $name }}
{{      trimSuffix "@I" $name -}}: {{ int $value | toString | toYaml }}
{{-   else if kindIs "string" $value }}
{{-     $value = include "afc.envValue" (dict "value" $value "Values" $.Values) }}
{{-     if ne $value "SKIP_SKIP_SKIP_SKIP" }}
{{        $name -}}: {{ $value | toString | toYaml }}
{{-     end }}
{{-   else }}
{{      $name -}}: {{ $value | toString | toYaml }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Renders environment entries from $.Values.component.*.env
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.componentEnvEntries" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- range $name, $value := default (dict) (get $compDef "env") }}
{{-   $skip := false }}
{{-   if hasSuffix "@S" $name }}
{{-     $name = trimSuffix "@S" $name }}
{{-   else if hasSuffix "@I" $name }}
{{-     $name = trimSuffix "@I" $name }}
{{      $value = int $value | toString }}
{{-   else if kindIs "string" $value }}
{{-     $value = include "afc.envValue" (dict "value" $value "Values" $.Values) }}
{{-     if eq $value "SKIP_SKIP_SKIP_SKIP" }}
{{        $skip = true }}
{{-     end }}
{{-   end }}
{{-   if not $skip }}
- name: {{ $name }}
  value: {{ $value | toString | toYaml }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Service port number by name
.component (key in $.Values.components) must be defined
.portName (key in components' port dictionary) must be defined
*/}}
{{- define "afc.servicePort" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $portDict := get $compDef "ports" | required (cat "Port dictionary not defined for this component:" .component) -}}
{{- $portDef := get $portDict .portName | required (cat "Component" .component "doesn' have port named" .portName) -}}
{{- $servicePort := get $portDef "servicePort" | required (cat "Component" .component "doesn't define service port number for port" .portName) -}}
{{- $servicePort -}}
{{- end -}}

{{/*
Container (target) port number by name
.component (key in $.Values.components) must be defined
.portName (key in components' port dictionary) must be defined
*/}}
{{- define "afc.containerPort" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $servicePort := include "afc.servicePort" . -}}
{{- default $servicePort (get (get (get $compDef "ports") .portName) "containerPort") -}}
{{- end -}}

{{/*
Declaration of static volumes (inhabitatnts of $.Values.staticVolumes) and mounted secrets in a Deployment/StatefulSet
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.staticVolumes" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $volumes := get $compDef "staticVolumeMounts" -}}
{{- $mountedSecrets := get $compDef "mountedSecrets" -}}
{{- if $volumes }}
{{-   range $name := keys $volumes }}
{{-     $volumeDef := get $.Values.staticVolumes $name | required (cat "Undefined static volume key" $name "used in definition of component" $.component) -}}
- name: {{ $name | include "afc.rfc1123" }} {{ toYaml $volumeDef | nindent 2 }}
{{    end }}
{{- end }}
{{- if $mountedSecrets }}
{{-   range $name := $mountedSecrets }}
{{-     if get (default (dict) $.Values.externalSecrets) $name }}
- name: {{ $name | include "afc.secretMountName" }}
  secret:
    secretName: {{ $name | include "afc.rfc1123" }}
{{-     end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Mount of static volumes (inhabitatnts of $.Values.staticVolumes) and mounted secrets in a Deployment/StatefulSet
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.staticVolumeMounts" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $volumes := get $compDef "staticVolumeMounts" -}}
{{- $mountedSecrets := get $compDef "mountedSecrets" -}}
{{- if $volumes }}
{{-   range $name, $mount := $volumes }}
{{-     $volumeDef := get $.Values.staticVolumes $name | required (cat "Undefined static volume key" $name "used in definition of component" $.component) -}}
- name: {{ $name | include "afc.rfc1123" }}
  mountPath: {{ $mount }}
{{    end }}
{{- end }}
{{- if $mountedSecrets }}
{{-   range $name := $mountedSecrets }}
{{-     if get (default (dict) $.Values.externalSecrets) $name }}
{{-       $mountPath := include "afc.secretMountPath" (dict "secret" $name "Values" $.Values) -}}
- name: {{ $name | include "afc.secretMountName" }}
  mountPath: {{ $mountPath }}
{{      end }}
{{-   end }}
{{- end }}
{{- end }}

{{/*
Common Deployment annotations
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.commonDeploymentAnnotations" -}}
kubectl.kubernetes.io/default-container: {{ include "afc.containerName" . }}
{{- end }}

{{/*
Common StatefulSet annotations
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.commonStatefulsetAnnotations" -}}
{{ include "afc.commonDeploymentAnnotations" . }}
{{- end }}

{{/*
Declaration of Prometheus metric endpoint for ServiceMonitor resource
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.metricEndpoints" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $metricsDef := get $compDef "metrics" | required (cat "Component" .component "does not have metrics' definition") -}}
- {{ toYaml $metricsDef | nindent 2 }}
{{- end }}

{{/*
Renders HPA min/maxReplicas
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.hpaReplicas" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $hpaDef := get $compDef "hpa" | required (cat "Component" .component "does not have 'hpa' section") -}}
minReplicas: {{ get $hpaDef "minReplicas" | default 1 | int }}
maxReplicas: {{ get $hpaDef "maxReplicas" | required (cat "Component" .component "does not have 'hpa.maxReplicas' value") | int }}
{{- end }}

{{/*
Renders HPA behavior section (if it is defined)
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.hpaBehavior" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $hpaDef := get $compDef "hpa" | required (cat "Component" .component "does not have 'hpa' section") -}}
{{- $behaviorDef := get $hpaDef "behavior" -}}
{{- if $behaviorDef }}
behavior: {{ toYaml $behaviorDef | nindent 2 }}
{{- end }}
{{- end }}

{{/*
Renders HPA metric
.component (key in $.Values.components) must be defined
*/}}
{{- define "afc.hpaMetric" -}}
{{- $compDef := merge (dict) (get $.Values.components .component | required (cat "No component for this component key:" .component)) $.Values.components.default -}}
{{- $hpaDef := get $compDef "hpa" | required (cat "Component" .component "does not have 'hpa' section") -}}
{{- $metricDef := get $hpaDef "metric" | required (cat "Component" .component "does not have 'hpa.metric' section") -}}
- type: {{ camelcase (mustFirst (keys $metricDef)) }}
  {{- toYaml $metricDef | nindent 2 }}
{{- end }}

{{/*
Renders secret store kind
.secretStore (key in $.Values.secretStores) must be defined
*/}}
{{- define "afc.extSecretStoreKind" -}}
{{- $extStoreDef := get $.Values.secretStores .secretStore | required (cat "Secret store" .secretStore "not defined in $.Values.secretStores") -}}
{{- hasKey $extStoreDef "namespace" | ternary "SecretStore" "ClusterSecretStore" -}}
{{- end }}

{{/*
Renders optional secret store namespace
.secretStore (key in $.Values.secretStores) must be defined
*/}}
{{- define "afc.extSecretStoreNamespace" -}}
{{- $extStoreDef := get $.Values.secretStores .secretStore | required (cat "Secret store" .secretStore "not defined in $.Values.secretStores") -}}
{{- $namespace := get $extStoreDef "namespace" -}}
{{- if $namespace }}
namespace: {{ $namespace }}
{{- end }}
{{- end }}

{{/*
Renders secretStoreRef in ExternalSecret
.secret (key in $.Values.externalSecrets) must be defined
*/}}
{{- define "afc.extSecretStoreRef" -}}
{{- $extSecretDef := merge (dict) (get $.Values.externalSecrets .secret | required (cat "External secret" .extSecret "not found")) $.Values.externalSecrets.default -}}
{{- $secretStoreName := get $extSecretDef "secretStore" | required (cat "secretStore not defined for external secret" .extSecret) -}}
secretStoreRef:
  name: {{ $secretStoreName }}
  kind: {{ include "afc.extSecretStoreKind" (dict "secretStore" $secretStoreName "Values" $.Values) }}
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
.secret (key in $.Values.externalSecrets) must be defined
*/}}
{{- define "afc.extSecretTarget" -}}
{{- $extSecretDef := merge (dict) (get $.Values.externalSecrets .secret | required (cat "External secret" .extSecret "not found")) $.Values.externalSecrets.default -}}
{{- $type := get $extSecretDef "type" -}}
target:
  name: {{ include "afc.secretManifestName" . }}
  creationPolicy: Owner
  deletionPolicy: Delete
{{- if $type }}
  template:
    type: {{ $type }}
{{- end }}
{{- end }}

{{/*
Renders data or dataFrom in ExternalSecret
.secret (key in $.Values.externalSecrets) must be defined
*/}}
{{- define "afc.extSecretData" -}}
{{- $extSecretDef := merge (dict) (get $.Values.externalSecrets .secret | required (cat "External secret" .secret "not found")) $.Values.externalSecrets.default -}}
{{- $property := get $extSecretDef "property" -}}
{{- $secretStoreName := get $extSecretDef "secretStore" | required  (cat "secretStore not defined for external secret" .secret) }}
{{- $secretStoreDef := get $.Values.secretStores $secretStoreName | required (cat "Secret" .secret "uses undefined secret store"  $secretStoreName) }}
{{- if $property }}
data:
  - secretKey: {{ $property }}
    remoteRef:
      key: {{ get $extSecretDef "remoteSecretName" | default .secret | include "afc.rfc1123" }}
{{-   if get $secretStoreDef "structured" }}
      property: {{ get $extSecretDef "remoteProperty" | default $property }}
{{-   end }}
{{- else }}
dataFrom:
  - key:  {{ get $extSecretDef "remoteSecretName" | default .secret | include "afc.rfc1123" }}
{{- end }}
{{- end }}

{{/*
Renders Ingress name
*/}}
{{- define "afc.ingressName" -}}
{{- $ingressDef := get $.Values "ingress" | required "'ingress' missing in values.yaml" -}}
{{ default "ingress" (get $ingressDef "name") | include "afc.rfc1123" }}
{{- end }}

{{/*
Renders ingressClassName (if specified)
*/}}
{{- define "afc.ingressClass" -}}
{{- $ingressDef := get $.Values "ingress" | required "'ingress' missing in values.yaml" -}}
{{- $ingressClassName := get $ingressDef "ingressClassName" -}}
{{- if $ingressClassName }}
ingressClassName: {{ $ingressClassName }}
{{- end }}
{{- end }}

{{/*
Renders ingress rules
*/}}
{{- define "afc.ingressRules" -}}
{{- $ingressDef := get $.Values "ingress" | required "'ingress' missing in values.yaml" -}}
- http:
    paths:
{{- range $ruleDef := (default (list) (get $ingressDef "rules")) }}
{{-   $compKey := get $ruleDef "componentKey" | required (cat "'componentKey' missing in ingress rule definition") }}
{{-   $portKey := get $ruleDef "portKey" | required (cat "'portKey' missing in ingress rule definition") }}
{{-   $path := get $ruleDef "path" | required (cat "'path' missing in ingress rule definition") }}
{{-   $compDef := get $.Values.components $compKey | required (cat "Component" $compKey "used in one of ingress rules not found") }}
{{-   $servicePort := get (default (dict) (get (default (dict) (get $compDef "ports")) $portKey)) "servicePort" | required (cat "Component" $compKey "used in one of ingress rules does not define service port" $portKey) }}
      - path: {{ $path }}
        pathType: {{ default "Prefix" (get $ruleDef "pathType") }}
        backend:
          service:
            name: {{ default $compKey (get (default (dict) (get $compDef "service")) "hostname" ) }}
            port:
              number: {{ $servicePort }}
{{- end }}
{{- end }}
