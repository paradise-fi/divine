Current Builds
==============

**Debug** builds have debug symbols, assertions enabled and no compiler
optimisations. **Release** builds have debug symbols, but no assertions and
compiler optimisations enabled.

Mainline
--------

<table class="hydra" id="mainline"></table>

Release Branch
--------------

Builds from the currently active release branch.

<table class="hydra" id="branch-3.1"></table>

Latest Changes
==============

<div id="changes">(loading, please wait)</div>

<!--
Latest Release
--------------

Builds of the latest release tarball. Only release-mode builds are available
here (no assertions, *no debug symbols*, compiler optimisation enabled):

<table id="release"></table>
-->

<script type="text/javascript" src="//ajax.googleapis.com/ajax/libs/jquery/1.8.3/jquery.min.js"></script>
<script type="text/javascript" src="hydra-api.js"></script>
<script type="text/javascript">
  var h = new Hydra('//divine.fi.muni.cz/hydra/', 'divine');

  $('#changes').load('//divine.fi.muni.cz/trac/timeline?max=10 #content > dl, #content > h2');
  h.fetch("mainline");
  h.fetch("branch-3.1");
</script>

