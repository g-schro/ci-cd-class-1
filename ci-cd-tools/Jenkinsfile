pipeline {
    agent any 

    environment {
        TOOL_DIR = "${WORKSPACE}\\ci-cd-tools"
    }

    stages {
        stage('Build') { 
            steps {
                bat "${TOOL_DIR}\\build.bat ${WORKSPACE}\\Debug Debug all"
                bat "${TOOL_DIR}\\build.bat ${WORKSPACE}\\Release Release all"
            }
        }

        stage('Static Code Analysis') {
            steps {
                catchError(buildResult: 'UNSTABLE', stageResult: 'FAILURE') {
                    bat "${TOOL_DIR}\\static-analysis.bat ${WORKSPACE}\\App"
                }
            }
        }

        stage('Flash-Debug') { 
            steps {
                bat "${TOOL_DIR}\\flash.bat $params.DUT_STLINK_sn ${WORKSPACE}\\Debug\\ci-cd-class-1.bin"
            }
        }

        stage('Test-Debug') { 
            steps {
                bat "python3 ${TOOL_DIR}\\base-hilt.py --dut-serial=$params.DUT_console --sim-serial=$params.SIM_console --tver=${BUILD_TAG}-Debug --jfile=test-results-debug.xml"
                junit 'test-results-debug.xml'
                bat "${TOOL_DIR}\\check-test-results.bat test-results-debug.xml"
            }
        }

        stage('Flash-Release') { 
            steps {
                bat "${TOOL_DIR}\\flash.bat $params.DUT_STLINK_sn ${WORKSPACE}\\Release\\ci-cd-class-1.bin"
            }
        }

        stage('Test-Release') { 
            steps {
                bat "python3 ${TOOL_DIR}\\base-hilt.py --dut-serial=$params.DUT_console --sim-serial=$params.SIM_console --tver=${BUILD_TAG}-Release --jfile=test-results-release.xml"
                junit 'test-results-release.xml'
                bat "${TOOL_DIR}\\check-test-results.bat test-results-release.xml"
            }
        }
    }

    post {
        success {
            bat "${TOOL_DIR}\\deliver.bat ${WORKSPACE} ${BUILD_TAG} ${GIT_COMMIT}"
        }
        unsuccessful {
            script {
                emailext (
                    to: '${DEFAULT_RECIPIENTS}',
                    subject: "Unsuccessful build ${env.BUILD_ID}",
                    body: "Build ${env.BUILD_ID} has result ${currentBuild.currentResult}"
                )
            }
        }
    }
}