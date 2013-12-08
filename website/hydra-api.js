function Hydra(base, project) {
    this.systems = [ "i686-linux", "x86_64-linux" ];
    this.variants = [ "debug", "release" ];
    this.flavours = ["normal", "cluster", "lvmetad", "snormal", "scluster", "slvmetad"];
    this.shortstat = { 'passed': '✓', 'good': '✓',
                       'warnings': '!',
                       'failed': 'x',
                       'skipped': '–',
                       'timeout': 'T' };

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
                              sel.jobset.replace(/-.*/, "") : sel.jobset) + ' ';
        if ( sel["job"] )
            s += '.job_' + sel.job + ' ';
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

    this.fetch = function(what)
    {
        var h = this;

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
            h.select({jobset: what}).append( row );
        }

        function getbuild(eval, job, sys, js) {

            function renderbuild(js, builds) {
                if ( builds.length == 0 ) return;
                var build = builds[0];
                var type = build.jobset.replace(/.*-/, "");
                var current = 0;
                for ( var i = 0; i < build.evals.length; ++i )
                    if ( eval == build.evals[i] )
                        current = 1;

                console.log( "build.job = " + build.job + ", type = " + type + ", system = " + build.system );
                h.select({ job: build.job, jobset: js, system: sys }).html(
                    (current ? "" : "(") +
                        '<a href="' + base + 'build/' + build.id + '" class="' +
                        h.bc( build.buildstatus ) + '">' +
                        h.bs( build.buildstatus ) + '</a>' + (current ? "" : ")") );
            }

            h.json( "latestbuilds", { nr: 1, project: project, system: sys, jobset: js,
                                      job: job }, function(b) { renderbuild( js, b ); } );
        }

        function rendertable(evid, jobs) {
            if ( jobs.length == 0 ) return;
            var tbl = '<tr><td class="job">job</td>';
            for ( var i = 0; i < h.systems.length; ++i )
                h.forvariants( what, function(js, v) {
                    tbl += '<td>' + h.systems[i] + ' ' + v + ' </td>';
                } );
            tbl += '</tr>';
            h.select({jobset: what}).html(tbl);
            for ( var i = 0; i < jobs.length; ++i ) {
                setupbuild( jobs[i].name );
                for ( var j = 0; j < h.systems.length; ++j )
                    h.forvariants( what, function(v) {
                        getbuild(evid, jobs[i].name, h.systems[j], v);
                    } );
            }
        }

        h.json( "latestevals", { nr: 1, project: project, jobset: h.basevariant( what ) }, 
                function(evals) {
                    if (!evals.length)
                        return; /* nothing to do */
                    console.log( "eval id = " + evals[0].id );
                    h.json("jobs", { project: project, jobset: h.basevariant( what ),
                                     eval: evals[0].id }, function(j) { rendertable( evals[0].id, j ); } );
                }
        );
    }

    function tr(id, testname, initial) {
        var $element = $('#' + id);
        if(!$element.length) {
                console.log( "create: " + id );
                $element = $('<tr onMouseOver="this.className = \'mouseover\'" ' +
                             '    onMouseOut="this.className = \'\'" ' +
                             'id="' + id + '">' + initial + '</tr>').appendTo('#results');
                $('#leftcol').append("<tr>" + td("testname",
                            testname.replace("shell/", "").replace(/.sh$/, "")) + "</tr>");
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
        $('#header').html('');
        $('#results').html('');
        $('#leftcol').html('');
        $('.synthcss').remove();

        function cutname(n) {
            if ( !n ) n = "?";
            return n.replace("_x86_64", " 64").replace("_i386", " 32").replace("rawhide", "rhide")
                .replace("clang_", "c ").replace("gcc_", "g ").replace("compression", "cpr")
                .replace("hashcompaction", "hc").replace("minimal", "min").replace("explicit", "exp")
                .replace("ubuntu", "ub").replace("fedora", "fc").replace("_small", "s");
        }

        function colhead(id, what) {
            return $("."+name[id]).filter(".hdr").html(
                      '<a href="' + base + "/build/" + id + '">' + cutname(name[id]) + '</a>: ' + what);
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
                colhead(build.id,
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
                    $td.html( div( build.job, // + " " + build.id,
                        '<a href="' + base + '/build/' + build.id + '/download/1/test-results/' +
                                  (fl ? fl + ':' : "") + file + '.txt">' +
                        h.shortstat[match[3]]+ '</a>' ) );
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
                        header2 += td( build.job ,  s[i] );
                    }
                } );

                $('#header').html('<thead><tr>' + header1 + '</tr>' +
                                  '<tr>' + header2 + '</tr></thead>');

                eachid( builds, function(build) {
                    colhead(build.id, "...");
                    addrule('.' + build.job + '{ width: .75em; }' );
                } );

                eachid( builds, function(build) {
                    var results;

                    $.get(base + '/build/' + build.id + "/download/1/test-results/list",
                        function(data) { oneresult(build, data); } ).fail( function() {
                            console.log( "failed: " + build.id );
                            colhead(build.id, '<a href="' + base + "/build/" +
                                build.id + '/log/raw">?</a>'); 
                            addrule("#results ." + build.job + " { background: lightgray; }");
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

    function scroller() {
        $("#scroller").scroll(function () { 
            $("#headerdiv").scrollLeft($("#scroller").scrollLeft());
            $("#resultsdiv").scrollLeft($("#scroller").scrollLeft());
        });

        $("#resultsdiv").scroll(function () { 
            $("#headerdiv").scrollLeft($("#resultsdiv").scrollLeft());
            $("#scroller").scrollLeft($("#resultsdiv").scrollLeft());
        });
    }

}
