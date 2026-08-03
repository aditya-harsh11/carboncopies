# Getting Started with BrainGenix

BrainGenix runs on Linux and is designed as a client-server platform: a backend stack (NES, API, and optional services) runs on one or more servers, and researchers interact with it remotely through the Python client. The client and backend do not need to be on the same machine.

This guide walks through the three steps to get a working installation.

## Steps

<div class="grid cards" markdown>

-   **:material-numeric-1-circle: System Requirements**

    ---

    Check hardware and OS requirements before installing. BrainGenix is developed and tested on Debian-based Linux (Debian 12, Ubuntu Server 22.04). A small single-machine setup needs at least 8 GB RAM. A production installation separates the API server and NES server, with NES requiring substantial RAM for large voxel array workloads.

    [:octicons-arrow-right-24: View Requirements](/Research/BrainGenix/GettingStarted/Requirements)

-   **:material-numeric-2-circle: Backend Installation**

    ---

    Set up the BrainGenix backend services on your server. This covers cloning the repositories, installing the API service, NES, and any supporting components. The backend is the computation layer that runs simulations and data acquisition pipelines.

    [:octicons-arrow-right-24: Backend Installation](/Research/BrainGenix/GettingStarted/BackendInstallation)

-   **:material-numeric-3-circle: Client Installation**

    ---

    Install `pyBrainGenixClient` on your local machine via pip. This Python package is the primary interface for building simulations, configuring virtual data acquisition, and interacting with everything the backend provides.

    [:octicons-arrow-right-24: Client Installation](/Research/BrainGenix/GettingStarted/ClientInstallation)

</div>

## Before You Begin

!!! note "Network architecture"
    The Python client connects to the backend over the network. You can run both on one machine for development, or point the client at a remote server for larger workloads. No special configuration is needed beyond knowing the backend's IP address or hostname.

!!! warning "OS compatibility"
    BrainGenix requires a native Linux installation. WSL (Windows Subsystem for Linux) is not supported. Ubuntu 24.04 is not currently supported; use Debian 12 or Ubuntu Server 22.04.

## Questions

For questions about installation or the platform, reach out at [contact@carboncopies.org](mailto:contact@carboncopies.org) or visit the [BrainGenix GitLab](https://gitlab.braingenix.org/carboncopies/).
