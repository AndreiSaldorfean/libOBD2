# libOBD2 release agent: release-please + gh + git + Claude Code + a C toolchain for assets.
# Build from the repository root:
#   docker build -f tools/release-agent/Dockerfile -t libobd2-release-agent .
FROM node:22-bookworm-slim

ARG RELEASE_PLEASE_VERSION=17.11.2
ARG CLAUDE_CODE_VERSION=2.1.321

RUN apt-get update \
 && apt-get install -y --no-install-recommends ca-certificates curl git python3 gcc libc6-dev make \
 && curl -fsSL https://cli.github.com/packages/githubcli-archive-keyring.gpg \
      -o /usr/share/keyrings/githubcli-archive-keyring.gpg \
 && echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/githubcli-archive-keyring.gpg] https://cli.github.com/packages stable main" \
      > /etc/apt/sources.list.d/github-cli.list \
 && apt-get update \
 && apt-get install -y --no-install-recommends gh \
 && rm -rf /var/lib/apt/lists/*

RUN npm install -g --no-fund --no-audit \
      release-please@${RELEASE_PLEASE_VERSION} \
      @anthropic-ai/claude-code@${CLAUDE_CODE_VERSION} \
 && npm cache clean --force \
 && git config --system --add safe.directory '*'

COPY tools/release-agent/release_agent.py scripts/changelog.py /opt/release-agent/

# Claude Code refuses to run some modes as root; the agent never needs root.
USER node
ENV HOME=/home/node WORKSPACE=/workspace
WORKDIR /workspace
ENTRYPOINT ["python3", "/opt/release-agent/release_agent.py"]
CMD ["help"]
