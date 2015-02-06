{
 # packages
 stdenv, fetchurl, qemu_kvm, wimlib, writeText, writeScript,
 vmTools,
 linux, utillinux, fuse, e2fsprogs,
 # pointer to a windows installation image
 iso,
 productKey ? "", # not really required or useful for our purposes
 language ? "en-US",
 architecture ? "x86",
 name ? "windows"
 }:

let origname = name;
 in stdenv.mkDerivation rec {
  inherit iso productKey language;
  name = origname + "-" + architecture;

  wimtools = wimlib;
  kvm = qemu_kvm;
  requiredSystemFeatures = [ "kvm" ];

  builder = writeScript "${name}-builder" ''
    source $stdenv/setup
    ensureDir $out
    set -x

    ${qemu_kvm}/bin/qemu-img create -f qcow2 $out/hda.img 16G
    ${qemu_kvm}/bin/qemu-kvm \
        -cdrom ${winpe_iso}/boot.iso \
        -drive file=$iso,index=3,media=cdrom \
        -hda $out/hda.img \
        -net nic -net user,restrict=on \
        -m 1024M -nographic
  '';

  machineName = "nixslave";
  password = "hookah"; # in possession of a large blue caterpillar

  bootscript = writeText "bootscript.cmd" ''
   drvload X:\windows\inf\msports.inf
   echo PASS: Boot > COM1
   e:
   setup.exe /unattend:D:\unattend.xml
   pause
  '';
  monitor = writeText "monitor.vbs" ''
    wsh.sleep 450000
    wscript.echo "PASS: Installer (interval " & wscript.arguments.item(0) & ")"
  '';
  monitor_cmd = "FOR %G IN (1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20) DO" +
                " cscript //nologo D:\monitor.vbs %G > COM1 2>&1";

  registry = writeText "registry.cmd" ''
    reg add HKLM\Software\Microsoft\Windows\CurrentVersion\Run /t REG_SZ /d "d:\batch.cmd"
    reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting" /v DontShowUI /t REG_DWORD /d 1
    reg add "HKLM\Software\Microsoft\Windows\Windows Error Reporting" /v Disabled /t REG_DWORD /d 1
  '';

  xmlKey = ''
      publicKeyToken="31bf3856ad364e35" language="neutral" versionScope="nonSxS"
  '';

  arch = ''processorArchitecture="${architecture}"'';

  winpe_iso = vmTools.runInLinuxVM (stdenv.mkDerivation {
    name = "winpe-iso";
    mountDisk = true;
    preVM = ''
      diskImage=$(pwd)/tmp.img
      ${qemu_kvm}/bin/qemu-img create -f raw $diskImage 1G
      ${e2fsprogs}/sbin/mkfs.ext2 -F $diskImage
    '';
    buildCommand = ''
      set -x

      ln -s ${linux}/lib /lib
      ensureDir $out

      mkdir /unpacked
      mkdir /work
      ${utillinux}/bin/mount ${iso} /unpacked

      cd /work
      mkdir tmp
      cp ${bootscript} install.cmd
      mkdir data
      cp ${unattend} data/unattend.xml
      cp ${monitor} data/monitor.vbs
      cp ${registry} data/registry.cmd
      echo shutdown -s > data/batch.cmd

      export PATH=$PATH:${fuse}/bin/ # for fusermount

      ${wimtools}/bin/mkwinpeimg \
         -i -W /unpacked \
         -s install.cmd \
         --img-overlay data \
         -t /work/tmp \
         $out/boot.iso
    '';
  });

  unattend = writeText "unattend.xml" ''
    <?xml version="1.0" encoding="utf-8"?>
    <unattend xmlns="urn:schemas-microsoft-com:unattend"
              xmlns:wcm="http://schemas.microsoft.com/WMIConfig/2002/State">
      <settings pass="windowsPE">

        <component name="Microsoft-Windows-International-Core-WinPE" ${arch} ${xmlKey}>
          <SetupUILanguage>
            <UILanguage>${language}</UILanguage>
          </SetupUILanguage>
          <InputLocale>0409:00000409</InputLocale>
          <UserLocale>${language}</UserLocale>
          <UILanguage>${language}</UILanguage>
          <SystemLocale>${language}</SystemLocale>
        </component>

        <component name="Microsoft-Windows-Setup" ${arch} ${xmlKey}>
          <DiskConfiguration>
              <Disk wcm:action="add">
                  <CreatePartitions>
                      <CreatePartition wcm:action="add">
                          <Order>1</Order>
                          <Extend>true</Extend>
                          <Type>Primary</Type>
                      </CreatePartition>
                  </CreatePartitions>
                  <ModifyPartitions>
                      <ModifyPartition wcm:action="add">
                          <Active>true</Active>
                          <Format>NTFS</Format>
                          <Label>System</Label>
                          <Order>1</Order>
                          <PartitionID>1</PartitionID>
                      </ModifyPartition>
                  </ModifyPartitions>
                  <DiskID>0</DiskID>
                  <WillWipeDisk>true</WillWipeDisk>
              </Disk>
          </DiskConfiguration>

          <ImageInstall>
              <OSImage>
                  <WillShowUI>OnError</WillShowUI>
                  <InstallTo>
                    <DiskID>0</DiskID>
                    <PartitionID>1</PartitionID>
                  </InstallTo>
                  <InstallFrom>
                    <Path>e:\sources\install.wim</Path>
                    <MetaData wcm:action="add">
                        <Key>/IMAGE/INDEX</Key>
                        <Value>4</Value>
                    </MetaData>
                  </InstallFrom>
              </OSImage>
          </ImageInstall>

          <UserData>
            <AcceptEula>true</AcceptEula>
            <FullName></FullName>
            <Organization></Organization>
            <ProductKey>
              <Key>${productKey}</Key>
              <WillShowUI>Never</WillShowUI>
            </ProductKey>
          </UserData>

          <RunSynchronous>
            <RunSynchronousCommand wcm:action="add">
              <Order>1</Order>
              <Path>cmd /c echo PASS: Windows PE > COM1</Path>
            </RunSynchronousCommand>
          </RunSynchronous>

          <RunAsynchronous>
            <RunAsynchronousCommand wcm:action="add">
              <Order>1</Order>
              <Path>cmd /c ${monitor_cmd}</Path>
            </RunAsynchronousCommand>
          </RunAsynchronous>
        </component>
      </settings>

      <settings pass="oobeSystem">
        <component name="Microsoft-Windows-Shell-Setup" ${arch} ${xmlKey}>
          <OOBE>
            <HideEULAPage>true</HideEULAPage>
            <NetworkLocation>Home</NetworkLocation>
            <ProtectYourPC>3</ProtectYourPC>
            <SkipMachineOOBE>true</SkipMachineOOBE>
            <SkipUserOOBE>true</SkipUserOOBE>
          </OOBE>
          <TimeZone>UTC</TimeZone>
          <UserAccounts>
            <AdministratorPassword>
              <Value>${password}</Value>
              <PlainText>true</PlainText>
            </AdministratorPassword>
            <LocalAccounts>
              <LocalAccount wcm:action="add">
                <Password>
                  <Value>${password}</Value>
                  <PlainText>true</PlainText>
                </Password>
                <Description>Default User</Description>
                <Group>Administrators;Users;</Group>
                <Name>default</Name>
              </LocalAccount>
            </LocalAccounts>
          </UserAccounts>
          <AutoLogon>
            <Password>
               <Value>${password}</Value>
               <PlainText>true</PlainText>
            </Password>
            <Username>Administrator</Username>
            <LogonCount>2</LogonCount>
            <Enabled>true</Enabled>
          </AutoLogon>
        </component>

        <component name="Microsoft-Windows-International-Core" ${arch} ${xmlKey}>
          <InputLocale>0409:00000409</InputLocale>
          <UserLocale>${language}</UserLocale>
          <UILanguage>${language}</UILanguage>
          <SystemLocale>${language}</SystemLocale>
        </component>
      </settings>

      <settings pass="specialize">
        <component name="Microsoft-Windows-Shell-Setup" ${arch} ${xmlKey}>
          <ComputerName>${machineName}</ComputerName>
          <CopyProfile>true</CopyProfile>
          <RegisteredOrganization></RegisteredOrganization>
          <RegisteredOwner></RegisteredOwner>
          <Display>
            <ColorDepth>32</ColorDepth>
            <HorizontalResolution>1024</HorizontalResolution>
            <RefreshRate>60</RefreshRate>
            <VerticalResolution>768</VerticalResolution>
          </Display>
        </component>

        <component name="Microsoft-Windows-Deployment" ${arch} ${xmlKey}>
          <RunSynchronous>
            <RunSynchronousCommand wcm:action="add">
              <Order>1</Order>
              <Path>cmd /c D:\registry.cmd > COM1</Path>
            </RunSynchronousCommand>
            <RunSynchronousCommand wcm:action="add">
              <Order>2</Order>
              <Path>cmd /c echo PASS: Deployment > COM1</Path>
            </RunSynchronousCommand>
          </RunSynchronous>
        </component>
      </settings>

    </unattend>
  '';

}
