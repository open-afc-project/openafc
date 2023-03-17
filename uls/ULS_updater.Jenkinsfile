timestamps {

    node ('uls') {
        stage ("Prerun cleanup") {
            sh """
            ls -al
            rm -rf * .dockerignore .git .github .gitignore
            ls -la
            """
        }

        stage ('ULS_Update - Checkout') {
            checkout(
                [
                    $class: 'GitSCM',
                    branches: [[name: '*/main']],
                    doGenerateSubmoduleConfigurations: false,
                    extensions: [],
                    submoduleCfg: [],
                    userRemoteConfigs: [[
                            credentialsId: GITHUB_CREDS_ID,
                            url: 'git@github.com:Telecominfraproject/open-afc.git'
                    ]]
                ]
            )
        }

        stage ('ULS_Update - Build') {
            sh """
            docker build . -t uls_updater:$BUILD_ID -f uls/Dockerfile-uls_updater
            """
        }

        stage ('ULS_Update - run') {
            sh """
            docker run -t -v $ULS_FOLDER:/output_folder -v $FS_DATA_FILES:/mnt/nfs/rat_transfer/daily_uls_parse/data_files --rm uls_updater:$BUILD_ID
            """
        }
    }
}