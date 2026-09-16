FROM ghcr.io/zephyrproject-rtos/zephyr-build:v0.29.2

# Additional tools (udev is a dependency for the serial monitor VS code extension,
# socat is used to bridge flash/debug/serial to a remote probe server on hosts
# where raw USB passthrough into the container is unavailable/unreliable, e.g.
# Docker Desktop on macOS)
RUN sudo apt update \
 && sudo apt install -y udev less bash-completion socat \
 && sudo apt clean

# Configure bash completion
RUN mkdir -p $HOME/.bash_completion.d \
 && echo 'source <(cd /workdir && west completion bash)' > $HOME/.bash_completion.d/west-completion.bash

# Install PyOCD package for STM32U3
RUN pyocd pack install STM32U3

# pyocd bug fix, needed regardless of host OS: tcp_client_probe.py's
# capabilities property returns None (instead of an empty list) before the
# remote probe is opened, crashing target init for anyone using pyocd's
# remote-probe protocol (`-u remote:...`).
RUN PYOCD_DIR="$(python3 -c 'import pyocd, os; print(os.path.dirname(pyocd.__file__))')" \
 && sudo sed -i \
      "s/return self._read_property('capabilities')/return self._read_property('capabilities', default=[])/" \
      "$PYOCD_DIR/probe/tcp_client_probe.py"
