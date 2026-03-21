pipeline {
    agent any
    environment {
        PATH = "/home/rudy/tools/arm-gnu-toolchain-15.2.rel1-x86_64-arm-none-eabi/bin:${env.PATH}"
    }
    stages {
        stage('Build on PR') {
            when {
                changeRequest()
            }
            steps {
                echo "Running for PR: ${env.CHANGE_ID}"
                sh './scripts/test-on-pr.sh'
            }
        }
    }
}
