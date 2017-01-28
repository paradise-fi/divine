DIVINE is **free software**, distributed under the **ISC licence** (2-clause
**BSD**). De­tailed licence texts may be found in the doc/COPYING file in the
distribution tarball. The distribution tarball includes freely redistributable
**third-party code**. Please refer to `doc/AUTHORS` and `doc/COPYING` included in the
distribution for co­pyright and licensing details.

Download
========

The latest release of DIVINE is **4.0**, available as a source tarball:
[divine-@version@.tar.gz](download/divine-@version@.tar.gz). You can also download
[nightly snapshots] [6] or fetch the source code from our version control
repository (you may need to install [darcs] [3]):

    $ darcs get http://divine.fi.muni.cz/current divine

To build the sources, you only need to run `make`. For a summary of changes
between releases, please consult our [release notes] [1] and for help with
installation, [detailed installation instructions] [4] are available in our
[manual] [5].

Alternatively, you can also download [older versions] [2]. Please note,
however, that DiVinE Cluster, DiVinE Multi-Core and ProbDiVinE have been
discontinued and the 3.x branch of DIVINE is no longer actively maintained
either.

Virtual Machine Images
----------------------

Since building DIVINE can be a time-consuming process, we provide pre-built VM
images for quick evaluation. You can download the latest image, either in form
of an [OVA Appliance] [7] (tested with VirtualBox), or [compressed VDI disk
image] [8] (tested with QEMU and VirtualBox, has to be extracted first, for
example using `unxz`). If you choose the VDI image, be sure to include serial
port in the configuration of the VM (it need not be connected) as the VM will
not start otherwise.

The easiest way to use the OVA Appliance is to import it to VirtualBox and add a
VirtualBox shared folder (machine's settings → shared folders) named
`divine-vbox` and then start the machine. It should then automatically mount
this folder to `/mnt/divine-vbox` and you can run DIVINE on files in this folder
(and edit them in your editor of choice outside of VirtualBox).

Some [older VM images](download/images/) are also available.

[1]: whatsnew.html
[2]: download
[3]: http://darcs.net
[4]: manual.html#installation
[5]: manual.html
[6]: download/snapshots/
[7]: download/images/divine-@version@.ova
[8]: download/images/divine-@version@.vdi.xz
