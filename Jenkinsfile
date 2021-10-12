/* Jenkinsfile to build and unit-test on CentOS.
 *
 * Environment variables required:
 * BRANCH_NAME to distinguish docker image names.
 * HOME to mirror the builder host file paths.
 */

properties([
    [$class: 'BuildDiscarderProperty', strategy: [$class: 'LogRotator',artifactDaysToKeepStr: '', artifactNumToKeepStr: '', daysToKeepStr: '',numToKeepStr: '30']],
    [$class: 'JobPropertyImpl', throttle: [count: 20,durationName: 'day']]
])

def buildtype = "RelWithDebInfo"
def dockerHost = 'repo.rkf-engineering.com:5000'

def getTrimmedShellOutput(command) {
    return sh(label: "shell", returnStdout: true, script: command).trim()
}

node('master') {
    stage('checkout') {
        cleanWs(disableDeferredWipeout: true, deleteDirs: true)
        // Get some code from a repository in fresh workspace
        checkout scm
        stash 'sourcetree'
        cleanWs()
    }
}

environs = [:]
environs["analysis"] = {
    node('linux-docker-host') {
        cleanWs(disableDeferredWipeout: true, deleteDirs: true)
        unstash 'sourcetree'
        def workingdir = "${pwd()}"
        
        docker.withRegistry("https://${dockerHost}", '513c9078-ee91-4cdf-9447-d90724e45648') {
            withEnv(['DOCKER_BUILDKIT=1']) {
                def dockerImage = "fbrat/${env.BRANCH_NAME}/analysis"
                def dockerEnv
                stage("build-image") {
                    sh "cp /etc/pki/tls/certs/ca-bundle.crt ."
            
                    def username = getTrimmedShellOutput("id -un")
                    def uid = getTrimmedShellOutput("id -u")
                    def groupname = getTrimmedShellOutput("id -gn")
                    def gid = getTrimmedShellOutput("id -g")
                    writeFile(
                        file: "Dockerfile",
                        text: """\
FROM centos:7.7.1908
# Builder-independent state
COPY ca-bundle.crt /etc/pki/tls/certs/ca-bundle.crt
RUN yum -y install epel-release
RUN yum-config-manager --add-repo https://repouser:pHp8k9Vg@repo.rkf-engineering.com/artifactory/list/rat-build/rat-build.repo
RUN yum -y install cpplint cppcheck pylint rubygem-puppet-lint sudo
RUN echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/wheel-nopasswd

# Mirror user account from builder host
# --no-log-init flag is required due to a bug in go https://github.com/golang/go/issues/13548
RUN groupadd -g ${gid} '${groupname}' && \
    useradd --no-log-init -u ${uid} -g ${gid} -G wheel '${username}'
ENV HOME=${env.HOME}
RUN chown -R ${uid}:${gid} \$HOME
USER jenkins
WORKDIR ${workingdir}
LABEL version="ci-${BUILD_NUMBER}"
"""
                    )
                    sh "docker pull ${dockerHost}/${dockerImage}:latest || true"
                    dockerEnv = docker.build("${dockerImage}:${env.BUILD_ID}", "--cache-from ${dockerHost}/${dockerImage}:latest --build-arg BUILDKIT_INLINE_CACHE=1 .")
                    dockerEnv.push("latest")
                }
                stage('analysis') {
                    dockerEnv.inside() {
                        sh 'cppcheck --quiet --max-configs=100 --enable=warning --xml ./src 2>cppcheck-error.xml'
                        sh 'cpplint --quiet --recursive ./src 2>cpplint-error.log || true'
                        sh 'pylint --rcfile=./pylintrc ./src/ratapi/ratapi >pylint.log || true'
                        sh 'puppet-lint --config=puppet/puppet-lint.rc --log-format "%{path}:%{line}:%{check}:%{KIND}:%{message}" puppet/site-modules >puppetlint.log || true'
                    }
                    recordIssues(
                        enabledForFailure: true,
                        ignoreQualityGate: true,
                        tools: [
                            cppCheck(pattern: 'cppcheck-error.xml'),
                        ],
                        unstableTotalAll: 1
                    )
                    recordIssues(
                        enabledForFailure: true,
                        ignoreQualityGate: true,
                        tools: [
                            cppLint(pattern: 'cpplint-error.log'),
                            pyLint(pattern: 'pylint.log'),
                            puppetLint(pattern: 'puppetlint.log'),
                            taskScanner(ignoreCase: true, includePattern: './src/*/*.h,./src/*/*.cpp', highTags: 'FIXME', normalTags: 'TODO'),
                        ],
                    )
                }
            }
        }
        cleanWs()
    }
}
environs["centos"] = {
    node('linux-docker-host') {
        cleanWs(disableDeferredWipeout: true, deleteDirs: true)
        unstash 'sourcetree'

        def workingdir = "${pwd()}"
        def apidocdir = "${workingdir}/testroot/share/doc/fbrat-api"

        def cmake = "cmake3 " +
            "-DCMAKE_INSTALL_PREFIX=${workingdir}/testroot " +
            "-DCMAKE_PREFIX_PATH=${workingdir}/testroot " +
            "-DAPIDOC_INSTALL_PATH=${apidocdir} " +
            "-DBOOST_INCLUDEDIR=/usr/include/boost169 -DBOOST_LIBRARYDIR=/usr/lib64/boost169 " +
            "-DBUILD_WITH_COVERAGE=on " +
            "-DCMAKE_BUILD_TYPE=${buildtype} " +
            "-G Ninja"
        def ninja = "ninja-build"

        stage('bolt') {
            sh "sh puppet/installpuppetfile.sh"
        }

        docker.withRegistry("https://${dockerHost}", '513c9078-ee91-4cdf-9447-d90724e45648') {
            withEnv(['DOCKER_BUILDKIT=1']) {
                def dockerImage = "fbrat/${env.BRANCH_NAME}/build"
                def dockerEnv
                stage("build-image") {
                    sh "cp /etc/pki/tls/certs/ca-bundle.crt ."
                    sh "cp ${env.HOME}/bin/runcmd.py ."
            
                    def username = getTrimmedShellOutput("id -un")
                    def uid = getTrimmedShellOutput("id -u")
                    def groupname = getTrimmedShellOutput("id -gn")
                    def gid = getTrimmedShellOutput("id -g")
                    writeFile(
                        file: "Dockerfile",
                        text: """\
FROM centos:7.7.1908
# Builder-independent state
COPY ca-bundle.crt /etc/pki/tls/certs/ca-bundle.crt
RUN yum-config-manager --add-repo https://repouser:pHp8k9Vg@repo.rkf-engineering.com/artifactory/list/rat-build/rat-build.repo
RUN yum -y install puppet-agent sudo
RUN update-alternatives --install /usr/bin/puppet puppet /opt/puppetlabs/puppet/bin/puppet 10
RUN yum -y clean all
RUN echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/wheel-nopasswd
COPY puppet/ /root/puppet
RUN sh /root/puppet/docker-apply.sh --color=off
COPY runcmd.py /usr/local/bin/runcmd.py
RUN chmod 755 /usr/local/bin/*

# Mirror user account from builder host
# --no-log-init flag is required due to a bug in go https://github.com/golang/go/issues/13548
RUN groupadd -g ${gid} '${groupname}' && \
    useradd --no-log-init -u ${uid} -g ${gid} -G wheel '${username}'
ENV HOME=${env.HOME}
RUN chown -R ${uid}:${gid} \$HOME
USER jenkins
WORKDIR ${workingdir}
LABEL version="fbrat-${BUILD_NUMBER}"
"""
                    )
                    sh "docker pull ${dockerHost}/${dockerImage}:latest || true"
                    dockerEnv = docker.build("${dockerImage}:${env.BUILD_ID}", "--cache-from ${dockerHost}/${dockerImage}:latest --build-arg BUILDKIT_INLINE_CACHE=1 .")
                    dockerEnv.push("latest")
                }
                stage('centos build') {
                    dockerEnv.inside() {
                        sh "mkdir -p build"
                        sh "mkdir -p ${apidocdir}"
                        dir("build"){
                            sh "runcmd.py cmake.log -- ${cmake} .."
                            sh "runcmd.py build.log -- ${ninja}"
                            sh "${ninja} install"
                            
                            sh "runcmd.py check.log -- ${ninja} check-coverage || true"
                            
                            sh 'doxygen >doxygen-out.log 2>doxygen-error.log || true'
                            
                        }
                    }
        
                    publishHTML(
                        target: [
                            allowMissing: true,
                            alwaysLinkToLastBuild: true,
                            keepAll: false,
                            reportDir: 'build/coverage/',
                            reportFiles: 'index.html',
                            reportName: 'Unit Test Coverage'
                        ]
                    )
                    publishHTML(
                        target: [
                            allowMissing: false,
                            alwaysLinkToLastBuild: true,
                            keepAll: false,
                            reportDir: "${apidocdir}",
                            reportFiles:'html/index.html',
                            reportName: 'API Doxygen'
                        ]
                    )
                    recordIssues(
                        ignoreQualityGate: true,
                        tools: [
                            doxygen(pattern: 'build/doxygen-error.log'),
                        ],
                    )
                    
                    recordIssues(
                        ignoreQualityGate: true,
                        tools: [
                            gcc4(pattern: 'build/build.log'), 
                        ],
                        unstableTotalAll: 1
                    )
                    xunit(
                        testTimeMargin: '3000',
                        thresholdMode: 1,
                        thresholds: [
                            failed(unstableNewThreshold: '', unstableThreshold: '0'),
                        ],
                        tools: [
                            GoogleTest(deleteOutputFiles: true, failIfNotNew: true,pattern: '**/**.junit.xml', skipNoTestFiles: true, stopProcessingIfError:false),
                            JUnit(deleteOutputFiles: true, failIfNotNew: true, pattern:'**/**.xunit.xml', skipNoTestFiles: true, stopProcessingIfError: false)
                        ]
                    )
                }
            }
        }
        cleanWs()
    }
}

parallel environs
