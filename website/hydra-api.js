function Hydra(base, project) {
    this.systems = [ "i686", "x86_64" ];
    this.variants = [ "debug", "release" ];
    this.flavours = ["normal", "cluster", "lvmetad", "snormal", "scluster", "slvmetad"];
    this.shortstat = { 'passed': '✓', 'good': '✓',
                       'warnings': '!',
                       'failed': 'x',
                       'skipped': '–',
                       'timeout': 'T',
                       'undef': '?' };

    this.basevariant = function(branch) {
        if (this.variants.length)
            return branch + '-' + this.variants[0];
        return branch;
    }

    this.forvariants = function(what, cb) {
        for ( var i = 0; i < this.variants.length; ++i )
            cb( what + '-' + this.variants[i], this.variants[i] );
        if ( !this.variants.length )
            cb( what, "" );
    }

    this.url = function(fun, param) {
        var u = base + '/api/' + fun + '?';
        for ( k in param ) {
            u += k + '=' + param[k] + '&';
        }
        return u;
    }

    this.json = function(fun, param, cb) {
        return $.getJSON( this.url( fun, param ), cb );
    }

    this.bs = function(s)
    {
        if ( s == 0 ) return "good";
        if ( s == 1 ) return "build error";
        if ( s == 2 ) return "dependency";
        if ( s == 3 ) return "aborted";
        if ( s == 4 ) return "4";
        if ( s == 5 ) return "5";
        if ( s == 6 ) return "tests failed";
        return "??";
    }

    this.bc = function(s)
    {
        if ( s == 0 ) return "good";
        if ( s == 1 ) return "build";
        if ( s == 2 ) return "deps";
        if ( s == 3 ) return "aborted";
        if ( s == 6 ) return "tests";
        return "";
    }

    this.select = function(sel) {
        var s = ""; // = ".hydra";
        if ( sel["jobset"] )
            s += '.hydra#' + (this.variants.length ?
                              sel.jobset.replace(/(.*)-[^-]*/, "$1") : sel.jobset).replace('.', "\\.") + ' ';
        else if ( sel["what"] )
            s += '.hydra#' + sel.what.replace('.', '\\.');

        if ( sel["job"] )
            s += '.job_' + sel.job.replace(/([^.]*).*/, "$1") + ' ';
        if ( sel["system"] )
            s += '.system_' + sel.system;
        if ( sel["variant"] )
            s += '.variant_' + sel.variant;
        else if ( sel["jobset"] && this.variants.length &&
                  sel.jobset.indexOf('-') >= 0 )
            s += '.variant_' + sel.jobset.replace(/.*-/, "");
        console.log( "selecting: " + s + ' l = ' + $(s).length );
        return $(s);
    }

    this.fixjob = function(j) {
        j.path = j.name;
        j.name = j.path.replace(/^([^.]*).*/, "$1");
        if ( j.path.indexOf(".") >= 0 ) {
            sys = j.path.replace(/^[^.]*\.(.*)$/, "$1");
            sys = sys.replace(/-linux/, "");
            if (sys == "i386")
                sys = "i686";
            j.system = sys;
        }
        return j;
    }

    this.fixjobs = function(js) {
        for ( var i = 0; i < js.length; i ++ )
            js[i] = this.fixjob( js[i] );
        return js;
    }

    this.fetch = function(what)
    {
        var h = this;
        var ev = new Object();

        function setupbuild(job) {
            var row = '<tr class="job_' + job + '">';
            row += '<td class="job">' + job + '</td>';
            for ( var i = 0; i < h.systems.length; ++i ) {
                if ( !h.variants.length )
                    row += '<td class="system_' + h.systems[i] + '"></td>';
                else for ( var j = 0; j < h.variants.length; ++j )
                    row += '<td class="system_' + h.systems[i] +
                           ' variant_' + h.variants[j] + '"></td>';
            }
            row += '</tr>';
            h.select({what: what}).append( row );
        }

        function getbuild(job, sys, js) {

            function renderbuild(js, builds) {
                if ( builds.length == 0 ) return;
                var build = builds[0];
                var type = build.jobset.replace(/.*-/, "");
                var current = 0;

                for ( var i = 0; i < ev.list.length; ++i )
                    for ( var j = 0; j < build.evals.length; ++j )
                        if ( ev.list[i].id == build.evals[j] )
                            current = 1;

                console.log( "build.job = " + build.job + ", type = " + type + ", system = " + build.system );
                h.select({ job: build.job, jobset: js, system: sys }).html(
                    (current ? "" : "(") +
                        '<a href="' + base + 'build/' + build.id + '" class="' +
                        h.bc( build.buildstatus ) + '">' +
                        h.bs( build.buildstatus ) + '</a>' + (current ? "" : ")") );
            }

            sel = { nr: 1, project: project, jobset: js, job: job };

            if ( job.indexOf(".") < 0 )
                sel["system"] = { i686: "i686-linux", x86_64: "x86_64-linux" }[sys];

            h.json( "latestbuilds", sel, function(b) { renderbuild( js, b ); } );
        }

        function rendertable(evid, jobs) {
            if ( jobs.length == 0 ) return;
            var tbl = '<tr><td class="job">job</td>';
            for ( var i = 0; i < h.systems.length; ++i )
                h.forvariants( what, function(js, v) {
                    tbl += '<td>' + h.systems[i] + ' ' + v + ' </td>';
                } );
            tbl += '</tr>';
            h.select({ what: what }).html(tbl);
            seen = new Object;
            for ( var i = 0; i < jobs.length; ++i ) {
                var name = jobs[i].name;
                if ( !seen[name] )
                    setupbuild( name );
                seen[name] = 1;
                for ( var j = 0; j < h.systems.length; ++j )
                    h.forvariants( what, function(v) {
                        if (jobs[i].system == h.systems[j] || !jobs[i].system)
                            getbuild(jobs[i].path, h.systems[j], v);
                    } );
            }
        }

        ev.list = new Array();
        ev.wait = $.Deferred();
        ev.fetched = 0;

        ev.wait.done( function() {
            h.json( "latestevals", { nr: 1, project: project, jobset: h.basevariant( what ) }, 
                function(evals) {
                    if (!evals.length)
                        return; /* nothing to do */
                    console.log( "eval id = " + evals[0].id );
                    h.json("jobs", { project: project, jobset: h.basevariant( what ),
                                     eval: evals[0].id }, function(j) {
                                         rendertable( evals[0].id, h.fixjobs( j ) ); } );
                }
            );
        } );

        h.forvariants(what, function(js, v) {
            h.json( "latestevals", { nr: 1, project: project, jobset: js }, function(evals) {
                if ( evals.length ) {
                    ev.list.push(evals[0]);
                    console.log( "fetched eval: " + evals[0].id + ", " + ev.fetched + " out of " + h.variants.length );
                }
                if ( ++ev.fetched == (h.variants.length ? h.variants.length : 1) )
                    ev.wait.resolve();
            } );
        } );
    }

    function tr(id, testname, initial) {
        var $element = $('#' + id);
        if(!$element.length) {
                console.log( "create: " + id );
                $element = $('<tr onMouseOver="this.className = \'mouseover\'" ' +
                             '    onMouseOut="this.className = \'\'" ' +
                             'id="' + id + '">' + td("testname",
                                 testname.replace("shell/", "").replace(/.sh$/, ""))
                             + initial + '</tr>').appendTo('#testmatrix');
        }
        return $element;
    }

    function td(cls, content) {
        return '<td class="' + cls + '">' + div(cls, content) + '</td>';
    }

    function div(cls, content) {
        return '<div class="' + cls + '">' + content + '</div>';
    }

    function eachprop(obj, fun) {
        for (var id in obj)
            fun(id);
    }

    function addrule(rule) {
        $("<style class='synthcss' type='text/css'>" + rule + "</style>").appendTo("head");
    }

    this.testmatrix = function(what) {
        var h = this;

        var sflavours = ["n", "c", "l", "N", "C", "L"];
        var ids = new Array();
        var name = new Object();
        var builds = new Object();

        // clear the table
        $('#testmatrix').html('');
        $('.synthcss').remove();

        function cutname(n) {
            if ( !n ) n = "?";
            return n.replace("_x86_64", " 64").replace("_i386", " 32").replace("rawhide", "rhide")
                .replace("clang_", "c ").replace("gcc_", "g ").replace("compression", "cpr")
                .replace("hashcompaction", "hc").replace("minimal", "min").replace("explicit", "exp")
                .replace("ubuntu", "ub").replace("fedora", "fc").replace("_small", "s");
        }

        function colhead(build, what) {
            return $("."+build.job).filter(".hdr").html(
                      '<a href="' + base + "/build/" + build.id + '">' +
                      cutname(build.job) + '</a>: ' + what);
        }

        function eachid(builds, fun) {
            for ( i = 0; i < builds.ids.length; ++i )
                if ( builds.ids[i] )
                    fun(builds[builds.ids[i]]);
        }

        function getbuilds(jobs, cb) {
            var jobcount = jobs.length;
            var builds = new Object();
            builds["ids"] = new Array();
            builds["name"] = new Object();

            jobs.sort( function(a, b) { return a.name.localeCompare( b.name ); } );

            for ( var i = 0; i < jobs.length; ++i ) {
                var job = jobs[i].name;
                console.log(job);
                h.json("latestbuilds", { nr: 1, project: project,
                                         jobset: h.basevariant( what ), job: job },
                    function(i) { return function(build) { 
                        if ( build.length ) {
                            builds.ids[i] = build[0].id;
                            builds.name[build[0].id] = build[0].job;
                            builds[build[0].id] = build[0];
                        }
                        if ( --jobcount == 0 )
                            cb(builds);
                    } }( i )
                );
            }
        }

        function initjobs(jobs) {
            var initial = "", header1 = "", // "<td>build:</td>",
                              header2 = ""; // td("testname", "test");
        }

        function processjobs(jobs) {
            var initial = "", header1 = "", // "<td>build:</td>",
                              header2 = ""; // td("testname", "test");

            function oneresult(build, data) {
                var pattern = h.flavours.length > 1 ?
                    /^([^ :]*):([^ ]*) (.*)$/mg : /^()([^ ]*) (.*)$/mg;
                colhead(build,
                    '<a href="' + base + "/build/" + build.id + '/download/2/coverage">c</a>' );
                while ( match = pattern.exec(data) ) {
                    var fl = match[1];
                    var test = match[2];
                    var file = test.replace("/", "_");
                    var testid = file.replace(".", "_");
                    var $tds = $("." + build.id, tr( testid, test, initial ) );
                    var sel = "";
                    if ( fl )
                        sel = "." + fl;
                    else if ( !h.flavours.length ) {
                        sel = "." + build.system;
                        /* if ( h.variants.length )
                            sel += "." + build.variant; */
                    }
                    var $td = sel ? $tds.filter(sel) : $tds;
                    var sst = h.shortstat[match[3]];
                    $td.html( div( build.job, // + " " + build.id,
                        '<a href="' + base + '/build/' + build.id + '/download/1/test-results/' +
                                  (fl ? fl + ':' : "") + file + '.txt" title="' +
                                  build.job + ": " + test + " on " + fl + '">' +
                        (sst ? sst : "?") + '</a>' ) );
                    $td.addClass( match[3] );
                }
            }

            getbuilds(jobs, function(builds) {
                eachid( builds, function(build) {
                    var n, s;
                    if ( h.flavours.length ) {
                        n = h.flavours;
                        s = sflavours;
                    } else {
                        n = h.systems;
                        s = h.systems.map( function(n) {
                            return n.replace("x86_64-linux", "64")
                                    .replace("i686-linux", "32"); } );
                    }

                    header1 += '<td colspan=' + n.length + ' class="hdr ' + build.job + '"></td>';
                    for ( var i = 0; i < n.length; ++i ) {
                        initial += td( build.id + ' ' + build.job + ' ' + n[i], "" );
                        header2 += td( build.job + ' ' + n[i],  s[i] );
                    }
                } );

                $('#testmatrix').html('<thead class="hdr"><tr><td>job →</td>' + header1 + '</tr>' +
                                      '<tr><td>test ↓</td>' + header2 + '</tr></thead>');

                eachid( builds, function(build) {
                    colhead(build, "...");
                    addrule('.' + build.job + '{ width: .75em; }' );
                } );

                eachid( builds, function(build) {
                    var results;

                    $.get(base + '/build/' + build.id + "/download/1/test-results/list",
                        function(data) { oneresult(build, data); } ).fail( function() {
                            console.log( "failed: " + build.id );
                            colhead(build, '<a href="' + base + "/build/" +
                                build.id + '/log/raw">?</a>'); 
                            addrule("#testmatrix." + build.job + " { background: lightgray; }");
                        } );
                    console.log( "loading: " + build.id );
                } );
            } );
        }

        h.forvariants( what, function(js, v) {
            h.json( "latestevals", { nr: 1, project: project, jobset: js }, 
                function(evals) {
                    if (!evals.length)
                        return; /* nothing to do */
                    h.json( "jobs", { project: project, jobset: js,
                        eval: evals[0].id }, processjobs );
                } ); } );
    }
}
