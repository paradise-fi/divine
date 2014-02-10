{stdenv, fetchurl, writeScript, p7zip, ruben ? true}:
 
let mingw = { pkg, hash }: fetchurl { url = "mirror://sourceforge/mingw/${pkg}"; sha256 = hash; };
 in stdenv.mkDerivation rec {
  name = "windows-mingw";

  builder = writeScript "${name}-builder" ''
    source $stdenv/setup
    mkdir -p $out
    mkdir mingw && cd mingw
    for f in $pkgs; do
       tar xaf $f || ${p7zip}/bin/7z x $f
       test -d mingw32 && cp -R mingw32/* .
       test -d mingw32-dw2 && cp -R mingw32-dw2/* .
       rm -rf mingw32 mingw32-dw2
    done
    echo "D:\\ /tools" > etc/fstab
    cd ..
    tar czf $out/package.tar.gz mingw
  '';

  unpack = "tar xzf $package/package.tar.gz";
  install = "set PATH=%PATH%;D:\\mingw\\bin";

  pkgs = toolchain ++ [ libgmp libintl_msys libintl_mingw libmpc libmpfr libiconv
           libregex libregex_msys libtermcap regex_dev tar
           bash make msys_core perl
           zlib libbz2 libcrypt libexpat sed grep coreutils coreutils_ext
           diffutils m4 mktemp ];

  libgmp = mingw {
    pkg = "libgmp-5.0.1-1-mingw32-dll-10.tar.lzma";
    hash = "0li69xmj2jj7yh2h19gwffjbzfaq45ckxn6wn6nd6a3g3lyn9q13"; };
  zlib = mingw {
    pkg = "zlib-1.2.3-2-msys-1.0.13-dll.tar.lzma";
    hash = "1lf0m2kg4mg5qdnyzspzp6zv94mrzf1wqhihlgav4a5r50498y21"; };
  libcrypt = mingw {
    pkg = "libcrypt-1.1_1-3-msys-1.0.13-dll-0.tar.lzma";
    hash = "067rjzswpfwwidkvccw8gviyj2cyp2w069b70ya8829mk6v5gw9i"; };
  libexpat = mingw {
    pkg = "libexpat-2.0.1-1-mingw32-dll-1.tar.gz";
    hash = "08rpm2hx4jbliqa5hg8zwy7figdpmbiqid8zrr14xgzc5smmflad"; };
  libbz2 = mingw {
    pkg = "libbz2-1.0.5-2-msys-1.0.13-dll-1.tar.lzma";
    hash = "0ksk6wh1lbm4dp7myjf1li1nij789c6fxdl4q6xnliy7qii0inks"; };
  regex_dev = mingw {
    pkg = "mingw-libgnurx-2.5.1-dev.tar.gz";
    hash = "0w07f17xndygg6vqsy5hcj71dikdaah2dls96mq0kdklh0nywrcc"; };
  sed = mingw {
    pkg = "sed-4.2.1-2-msys-1.0.13-bin.tar.lzma";
    hash = "150vbp7fs3kf6aif7ip5q18bzkmzrg0bf201hkkr3dpc9hh5jc7p"; };
  grep = mingw {
    pkg = "grep-2.5.4-2-msys-1.0.13-bin.tar.lzma";
    hash = "1fj00k4nmrri71fwsy2pw1vwxyx7psrzm3rfcaabk3gr9mss2hj8"; };
  tar = mingw {
    pkg = "tar-1.23-1-msys-1.0.13-bin.tar.lzma";
    hash = "11r818jkd3sfxflv9r6v2vj9vy69j5l2fnx6fy061ab3jmd9x81g"; };
  coreutils = mingw {
    pkg = "coreutils-5.97-3-msys-1.0.13-bin.tar.lzma";
    hash = "0ri31ci1952hlgw8rmdj4j3npi3bb47zxp1nqd5af5pa2q29kizq"; };
  coreutils_ext = mingw {
    pkg = "coreutils-5.97-3-msys-1.0.13-ext.tar.lzma";
    hash = "0ya5w30syxcq9mkp9lm0lhp9hkr4sdb0mpvdcpyrzxsgr6k5lliz"; };
  diffutils = mingw {
    pkg = "diffutils-2.8.7.20071206cvs-3-msys-1.0.13-bin.tar.lzma";
    hash = "0r5qm4z42j83cslbhpyaabri2nlrc9ibllj7ghrx4ba98jq8ja2j"; };
  m4 = mingw {
    pkg = "m4-1.4.14-1-msys-1.0.13-bin.tar.lzma";
    hash = "0gp3hx2avkxkq2shsrizx5fbswg09sna9wbrv7yh3bcilv4qn1a1"; };

  libintl_mingw = mingw {
    pkg = "libintl-0.18.1.1-2-mingw32-dll-8.tar.lzma";
    hash = "0jandbdd0ijd6gzbafxb9s35931clzr9pz1zvdnvqg933h5zk40k"; };
  libintl_msys = mingw {
    pkg = "libintl-0.18.1.1-1-msys-1.0.17-dll-8.tar.lzma";
    hash = "0439q7g71ymwvpb7aal4ap2zkickr4jsnhd3wbxi3ib1jsb8rnr9"; };
  libmpc = mingw {
    pkg = "libmpc-0.8.1-1-mingw32-dll-2.tar.lzma";
    hash = "1dim364a05gyrkjgmb194fnqsb0b4qz8rssk8dx795jym0vbqivh"; };
  libmpfr = mingw {
    pkg = "libmpfr-2.4.1-1-mingw32-dll-1.tar.lzma";
    hash = "0h8a8g10d5dd8sja83ncccms3yx12mfxc1v7h2gwsywvbs2nm8v4"; };
  libiconv = mingw {
    pkg = "libiconv-1.14-1-msys-1.0.17-dll-2.tar.lzma";
    hash = "10qn4csg19kvqa6hr7s1j6dd5clsippbjlk8da79q99jqbl22s8r"; };
  libregex = mingw {
    pkg = "mingw-libgnurx-2.5.1-bin.tar.gz";
    hash = "07haaf0xs481phv8wv76ghm972f6lpwcbx8a7gl65m1rckwq2bri"; };
  libregex_msys = mingw {
    pkg = "libregex-1.20090805-2-msys-1.0.13-dll-1.tar.lzma";
    hash = "1lfk9c9vx5n8hkr1jbfdx3fmbfz2gax5ggk7z32pa1m94wg8rpc5"; };
  libgcc = mingw {
    pkg = "libgcc-4.7.0-1-mingw32-dll-1.tar.lzma";
    hash = "00k1nnv0vd22hn03hbgvc2vvv63fcb60pf1xi9h7h98hhf26jss6"; };
  libtermcap = mingw {
    pkg = "libtermcap-0.20050421_1-2-msys-1.0.13-dll-0.tar.lzma";
    hash = "1dmdyxqgh9dx4w18g1ai3jf460lvk1jr50aar3y7428gi3h8zdb2"; };
  gcc_core = mingw {
    pkg = "gcc-core-4.7.0-1-mingw32-bin.tar.lzma";
    hash = "140xnxvf2isnq6ib9jbg8fxj7bqj3yy239nqaivxjbwi0h0vdnv7"; };
  gcc_cpp = mingw {
    pkg = "gcc-c++-4.7.0-1-mingw32-bin.tar.lzma";
    hash = "1mplyp5x83fi27lqf18d2lnjwsg5lvrpsckfx1nvgpv6bcrimnix"; };
  gcc_libstdcpp = mingw {
    pkg = "libstdc++-4.7.0-1-mingw32-dll-6.tar.lzma";
    hash = "19m1qj96awlviigb5xhn305n96d9bkawg18x16y2h4nc1rdmx33q"; };
  binutils = mingw {
    pkg = "binutils-2.22-1-mingw32-bin.tar.lzma";
    hash = "0a9cq0kpkb53b4fzxjhk93zcjpl1czj4adihv7n7gf3jj2in7n35"; };
  bash = mingw {
    pkg = "bash-3.1.17-4-msys-1.0.16-bin.tar.lzma";
    hash = "09h1y7fc5jpwlyy0pzj2159ajdzs6vdx9j7xnsyqz30kfzgi3snn"; };
  make = mingw {
    pkg = "make-3.81-3-msys-1.0.13-bin.tar.lzma";
    hash = "124rp2lnka3zk4y7aa6qda0ix0mi54nnkgv7iqf80dbiy2xhqzw4"; };
  msys_core = mingw {
    pkg = "msysCORE-1.0.17-1-msys-1.0.17-bin.tar.lzma";
    hash = "0zq3ichd7vqrbd5fx9383znq66mysj3dizx1303plygmjkiplw1d"; };
  perl = mingw {
    pkg = "perl-5.8.8-1-msys-1.0.17-bin.tar.lzma";
    hash = "16bs69m018b9ri6qzsbx09klyw75rq4aklh5841xswh1w2f96ywq"; };
  w32api = mingw {
    pkg = "w32api-3.17-2-mingw32-dev.tar.lzma";
    hash = "02h936464fflddcmcq4j5yizq12b75w4772im923pcgdkdvn0av7"; };
  mingw_rt = mingw {
    pkg = "mingwrt-3.20-mingw32-dev.tar.gz";
    hash = "1wi850pirpc10sfrvmglg11gkh5vmr53hkh3lc42agh9371if5x4"; };
  mktemp = mingw {
    pkg = "mktemp-1.6-2-msys-1.0.13-bin.tar.lzma";
    hash = "1363102arak4x2cd7z0zl70jjrjkirr1w619gcj7pj8vy8pqky68";
  };

  toolchain =
    if ruben
       then [ (fetchurl {
                url = "mirror://sourceforge/mingw-w64/i686-w64-mingw32-gcc-dw2-4.8-stdthread-win32_rubenvb.7z";
                sha256 = "1csgfa4qb1gmn1bcfafq2xvi805agdx4gld7js3hpah8fdpxiv9z"; }) ]
       else [ gcc_core gcc_cpp gcc_libstdcpp binutils w32api mingw_rt ];

}
