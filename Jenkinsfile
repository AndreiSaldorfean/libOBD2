// CI for libOBD2 (#69): runs on every PR into develop/main and on pushes to develop and main.
// Each check is one script under scripts/ci/ that runs the same way on a developer machine:
//   ci/build-matrix  python3 scripts/ci/build_matrix.py   every port with every app it supports
//   ci/unit-tests    scripts/ci/unit_tests.sh             host_virtual unit tests, 0 failures
//   ci/coverage      scripts/ci/coverage.sh               coverage never below tests/coverage_baseline.json
// Each check posts its own GitHub commit status; branch protection on develop and main requires all
// three. A failing check does not stop the others, so a PR sees every problem in one run.
//
// Node requirements: cmake, make, gcc/gcov, python3 >= 3.8, git, and the Arm GNU toolchain.
// Set ARM_TOOLCHAIN_BIN (Manage Jenkins > System > Global properties) to its bin/ directory.
// Statuses are posted with a username + token credential with commit-status write: the id in
// GITHUB_STATUS_CREDENTIALS (global property), default 'github-status'. Without it the checks still
// run and only the GitHub statuses are skipped.

// The commit GitHub shows the PR with: for a PR built as a merge with its target, the PR head.
// Jenkins checks out the PR head and merges the target into it, so the merge's parents are
// [PR head, target]: take the parent that is not the target's tip (#86).
def headSha() {
    def parents = sh(script: 'git rev-list --parents -n 1 HEAD', returnStdout: true).trim().split(' ')
    if (!env.CHANGE_ID || parents.size() <= 2) {
        return parents[0]
    }
    def target = sh(script: "git rev-parse --verify -q 'refs/remotes/origin/${env.CHANGE_TARGET}^{commit}' || true",
                    returnStdout: true).trim()
    for (int i = 1; i < parents.size(); i++) {
        if (parents[i] != target) {
            return parents[i]
        }
    }
    return parents[1]
}

def repoSlug() {
    def url = sh(script: 'git config --get remote.origin.url', returnStdout: true).trim()
    def m = (url =~ /github\.com[:\/]([^\/]+\/[^\/]+?)(\.git)?$/)
    return m ? m[0][1] : ''
}

def githubStatus(String context, String state, String description) {
    if (!env.CI_HEAD_SHA || !env.CI_REPO) { return }
    try {
        withCredentials([usernamePassword(credentialsId: env.GITHUB_STATUS_CREDENTIALS ?: 'github-status', usernameVariable: 'GH_USER', passwordVariable: 'GH_TOKEN')]) {
            withEnv(["CTX=${context}", "STATE=${state}", "DESC=${description}"]) {
                sh(label: "GitHub status ${context}: ${state}", script: '''#!/bin/bash
                    set +x
                    body=$(python3 -c 'import json, os; print(json.dumps({"state": os.environ["STATE"], "context": os.environ["CTX"], "description": os.environ["DESC"][:140], "target_url": os.environ.get("RUN_DISPLAY_URL") or os.environ.get("BUILD_URL", "")}))')
                    curl -fsS -o /dev/null -X POST -H "Authorization: Bearer $GH_TOKEN" -H "Accept: application/vnd.github+json" \
                        "https://api.github.com/repos/$CI_REPO/statuses/$CI_HEAD_SHA" -d "$body"
                ''')
            }
        }
    } catch (err) {
        echo "GitHub status ${context} not posted: ${err.getMessage()}"
    }
}

// Run one check: pending -> success/failure on GitHub; a failure marks the stage and the build
// failed but lets the next stages run.
def runCheck(String context, String command, def okText, String failText) {
    githubStatus(context, 'pending', 'Running')
    def rc = sh(script: command, returnStatus: true)
    if (rc == 0) {
        githubStatus(context, 'success', okText instanceof Closure ? okText() : okText)
    } else {
        githubStatus(context, 'failure', failText)
        catchError(buildResult: 'FAILURE', stageResult: 'FAILURE') {
            error("${context} failed (exit ${rc}): ${failText}")
        }
    }
}

pipeline {
    agent any
    options {
        timestamps()
        timeout(time: 90, unit: 'MINUTES')
        buildDiscarder(logRotator(numToKeepStr: '30', artifactNumToKeepStr: '10'))
        disableConcurrentBuilds(abortPrevious: true)
        skipDefaultCheckout()
    }
    stages {
        stage('Checkout') {
            steps {
                checkout scm
                sh 'git submodule sync --recursive && git submodule update --init --recursive --depth 1'
                script {
                    env.CI_HEAD_SHA = headSha()
                    env.CI_REPO = repoSlug()
                    ['ci/build-matrix', 'ci/unit-tests', 'ci/coverage'].each { githubStatus(it, 'pending', 'Queued') }
                }
            }
        }
        stage('Build matrix') {
            steps {
                script {
                    runCheck('ci/build-matrix', 'python3 scripts/ci/build_matrix.py',
                             'Every port and app builds', 'A port or app does not build: see the build-matrix tests')
                }
            }
            post {
                always {
                    junit allowEmptyResults: true, testResults: 'builds/ci/build_matrix.xml'
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'builds/ci/logs/*.log'
                }
            }
        }
        stage('Unit tests (host_virtual)') {
            steps {
                script {
                    runCheck('ci/unit-tests', 'scripts/ci/unit_tests.sh',
                             'All host_virtual unit tests pass', 'host_virtual unit tests fail, or more tests are ignored')
                }
            }
            post {
                always {
                    junit allowEmptyResults: true, testResults: 'builds/ci/unit_tests.xml'
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'builds/ci/unit_tests.txt, test_results/**'
                }
            }
        }
        stage('Coverage') {
            steps {
                script {
                    runCheck('ci/coverage', 'scripts/ci/coverage.sh', {
                        def t = readJSON_total()
                        return "lines ${t.lines}%, branches ${t.branches}%, functions ${t.functions}% (not below baseline)"
                    }, 'Coverage dropped below tests/coverage_baseline.json')
                }
            }
            post {
                always {
                    archiveArtifacts allowEmptyArchive: true, artifacts: 'builds/ci/coverage.txt, builds/ci/coverage.json'
                }
            }
        }
    }
    post {
        aborted {
            script { ['ci/build-matrix', 'ci/unit-tests', 'ci/coverage'].each { githubStatus(it, 'error', 'Build aborted') } }
        }
    }
}

// Totals from builds/ci/coverage.json without the pipeline-utility-steps plugin.
def readJSON_total() {
    def out = sh(returnStdout: true, script: '''python3 -c 'import json; t = json.load(open("builds/ci/coverage.json"))["total"]; print(" ".join(str(t[m]["percent"]) for m in ("lines", "branches", "functions")))' ''').trim().split(' ')
    return [lines: out[0], branches: out[1], functions: out[2]]
}
