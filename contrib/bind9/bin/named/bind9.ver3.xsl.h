/*
 * Generated by convertxsl.pl 1.14 2008/07/17 23:43:26 jinmei Exp  
 * From <!-- %Id: bind9.xsl 1.21 2009/01/27 23:47:54 tbox Exp %
 */
static char xslmsg[] =
	"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
	"<!--\n"
	" - Copyright (C) 2012-2014 Internet Systems Consortium, Inc. (\"ISC\")\n"
	" -\n"
	" - Permission to use, copy, modify, and/or distribute this software for any\n"
	" - purpose with or without fee is hereby granted, provided that the above\n"
	" - copyright notice and this permission notice appear in all copies.\n"
	" -\n"
	" - THE SOFTWARE IS PROVIDED \"AS IS\" AND ISC DISCLAIMS ALL WARRANTIES WITH\n"
	" - REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY\n"
	" - AND FITNESS. IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,\n"
	" - INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM\n"
	" - LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE\n"
	" - OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR\n"
	" - PERFORMANCE OF THIS SOFTWARE.\n"
	"-->\n"
	"\n"
	"<!-- $Id$ -->\n"
	"\n"
	"<!-- \045Id: bind9.xsl,v 1.21 2009/01/27 23:47:54 tbox Exp \045 -->\n"
	"<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" xmlns=\"http://www.w3.org/1999/xhtml\" version=\"1.0\">\n"
	" <xsl:output method=\"html\" indent=\"yes\" version=\"4.0\"/>\n"
	" <xsl:template match=\"statistics[@version=&quot;3.3&quot;]\">\n"
	" <html>\n"
	" <head>\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <script type=\"text/javascript\" src=\"https://www.google.com/jsapi\"/>\n"
	" <script type=\"text/javascript\">\n"
	"\n"
	" google.load(\"visualization\", \"1\", {packages:[\"corechart\"]});\n"
	" google.setOnLoadCallback(loadGraphs);\n"
	"\n"
	" var graphs=[];\n"
	"\n"
	" function drawChart(chart_title,target,style,data) {\n"
	" var data = google.visualization.arrayToDataTable(data);\n"
	"\n"
	" var options = {\n"
	" title: chart_title\n"
	" };\n"
	"\n"
	" var chart;\n"
	" if (style == \"barchart\") {\n"
	" chart = new google.visualization.BarChart(document.getElementById(target));\n"
	" chart.draw(data, options);\n"
	" } else if (style == \"piechart\") {\n"
	" chart = new google.visualization.PieChart(document.getElementById(target));\n"
	" chart.draw(data, options);\n"
	" }\n"
	" }\n"
	"\n"
	" function loadGraphs(){\n"
	" var g;\n"
	"\n"
	" while(g = graphs.shift()){\n"
	" // alert(\"going for: \" + g.target);\n"
	" if(g.data.length > 1){\n"
	" drawChart(g.title,g.target,g.style,g.data);\n"
	" }\n"
	" }\n"
	" }\n"
	"\n"
	" // Server Incoming Query Types \n"
	" graphs.push({\n"
	" 'title' : \"Server Incoming Query Types\",\n"
	" 'target': 'chart_incoming_qtypes',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Type','Counter'],<xsl:for-each select=\"server/counters[@type=&quot;qtype&quot;]/counter\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]\n"
	" });\n"
	"\n"
	" // Server Incoming Requests by opcode\n"
	" graphs.push({\n"
	" 'title' : \"Server Incoming Requests by DNS Opcode\",\n"
	" 'target': 'chart_incoming_opcodes',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Opcode','Counter'],<xsl:for-each select=\"server/counters[@type=&quot;opcode&quot;]/counter[. &gt; 0 or substring(@name,1,3) != 'RES']\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]});\n"
	" </script>\n"
	" </xsl:if>\n"
	" <style type=\"text/css\">\n"
	" body {\n"
	" font-family: sans-serif;\n"
	" background-color: #ffffff;\n"
	" color: #000000;\n"
	" font-size: 10pt;\n"
	" }\n"
	"\n"
	" .odd{\n"
	" background-color: #f0f0f0;\n"
	" }\n"
	"\n"
	" .even{\n"
	" background-color: #ffffff;\n"
	" }\n"
	"\n"
	" p.footer{\n"
	" font-style:italic;\n"
	" color: grey;\n"
	" }\n"
	"\n"
	" table {\n"
	" border-collapse: collapse;\n"
	" border: 1px solid grey;\n"
	" }\n"
	"\n"
	" table.counters{\n"
	" border: 1px solid grey;\n"
	" width: 500px;\n"
	" }\n"
	" table.counters th {\n"
	" text-align: right;\n"
	" border: 1px solid grey;\n"
	" width: 150px;\n"
	" }\n"
	" table.counters td {\n"
	" text-align: right;\n"
	" font-family: monospace;\n"
	" }\n"
	" table.counters tr:hover{\n"
	" background-color: #99ddff;\n"
	" }\n"
	"\n"
	" table.info {\n"
	" border: 1px solid grey;\n"
	" width: 500px;\n"
	" }\n"
	" table.info th {\n"
	" text-align: center;\n"
	" border: 1px solid grey;\n"
	" width: 150px;\n"
	" }\n"
	" table.info td {\n"
	" text-align: center;\n"
	" }\n"
	" table.info tr:hover{\n"
	" background-color: #99ddff;\n"
	" }\n"
	"\n"
	" table.tasks {\n"
	" border: 1px solid grey;\n"
	" width: 500px;\n"
	" }\n"
	" table.tasks th {\n"
	" text-align: center;\n"
	" border: 1px solid grey;\n"
	" width: 150px;\n"
	" }\n"
	" table.tasks td {\n"
	" text-align: right;\n"
	" font-family: monospace;\n"
	" }\n"
	" table.tasks td:nth-child(2) {\n"
	" text-align: center;\n"
	" }\n"
	" table.tasks td:nth-child(4) {\n"
	" text-align: center;\n"
	" }\n"
	" table.tasks tr:hover{\n"
	" background-color: #99ddff;\n"
	" }\n"
	"\n"
	" table.netstat {\n"
	" border: 1px solid grey;\n"
	" width: 500px;\n"
	" }\n"
	" table.netstat th {\n"
	" text-align: center;\n"
	" border: 1px solid grey;\n"
	" width: 150px;\n"
	" }\n"
	" table.netstat td {\n"
	" text-align: center;\n"
	" }\n"
	" table.netstat td:nth-child(4) {\n"
	" text-align: right;\n"
	" font-family: monospace;\n"
	" }\n"
	" table.netstat td:nth-child(7) {\n"
	" text-align: left;\n"
	" }\n"
	" table.netstat tr:hover{\n"
	" background-color: #99ddff;\n"
	" }\n"
	"\n"
	" table.mctx {\n"
	" border: 1px solid grey;\n"
	" width: 500px;\n"
	" }\n"
	" table.mctx th {\n"
	" text-align: center;\n"
	" border: 1px solid grey;\n"
	" }\n"
	" table.mctx td {\n"
	" text-align: right;\n"
	" font-family: monospace;\n"
	" }\n"
	" table.mctx td:nth-child(-n+2) {\n"
	" text-align: left;\n"
	" width: 100px;\n"
	" }\n"
	" table.mctx tr:hover{\n"
	" background-color: #99ddff;\n"
	" }\n"
	"\n"
	" .totals {\n"
	" background-color: rgb(1,169,206);\n"
	" color: #ffffff;\n"
	" }\n"
	"\n"
	" td, th {\n"
	" padding-right: 5px;\n"
	" padding-left: 5px;\n"
	" border: 1px solid grey;\n"
	" }\n"
	"\n"
	" .header h1 {\n"
	" color: rgb(1,169,206);\n"
	" padding: 0px;\n"
	" }\n"
	"\n"
	" .content {\n"
	" background-color: #ffffff;\n"
	" color: #000000;\n"
	" padding: 4px;\n"
	" }\n"
	"\n"
	" .item {\n"
	" padding: 4px;\n"
	" text-align: right;\n"
	" }\n"
	"\n"
	" .value {\n"
	" padding: 4px;\n"
	" font-weight: bold;\n"
	" }\n"
	"\n"
	"\n"
	" h2 {\n"
	" color: grey;\n"
	" font-size: 14pt;\n"
	" width:500px;\n"
	" text-align:center;\n"
	" }\n"
	"\n"
	" h3 {\n"
	" color: #444444;\n"
	" font-size: 12pt;\n"
	" width:500px;\n"
	" text-align:center;\n"
	" }\n"
	" h4 {\n"
	" color: rgb(1,169,206);\n"
	" font-size: 10pt;\n"
	" width:500px;\n"
	" text-align:center;\n"
	" }\n"
	"\n"
	" .pie {\n"
	" width:500px;\n"
	" height: 500px;\n"
	" }\n"
	"\n"
	" </style>\n"
	" <title>ISC BIND 9 Statistics</title>\n"
	" </head>\n"
	" <body>\n"
	" <div class=\"header\">\n"
	" <h1>ISC Bind 9 Configuration and Statistics</h1>\n"
	" </div>\n"
	" <p>Alternate statistics views: <a href=\"/\">All</a>,\n"
	" <a href=\"/xml/v3/status\">Status</a>,\n"
	" <a href=\"/xml/v3/server\">Server</a>,\n"
	" <a href=\"/xml/v3/zones\">Zones</a>,\n"
	" <a href=\"/xml/v3/net\">Network</a>,\n"
	" <a href=\"/xml/v3/tasks\">Tasks</a> and\n"
	" <a href=\"/xml/v3/mem\">Memory</a></p>\n"
	" <hr/>\n"
	" <h2>Server Times</h2>\n"
	" <table class=\"info\">\n"
	" <tr>\n"
	" <th>Boot time:</th>\n"
	" <td>\n"
	" <xsl:value-of select=\"server/boot-time\"/>\n"
	" </td>\n"
	" </tr>\n"
	" <tr>\n"
	" <th>Current time:</th>\n"
	" <td>\n"
	" <xsl:value-of select=\"server/current-time\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </table>\n"
	" <br/>\n"
	" <xsl:if test=\"server/counters[@type=&quot;opcode&quot;]/counter[. &gt; 0]\">\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <h2>Incoming Requests by DNS Opcode</h2>\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <div class=\"pie\" id=\"chart_incoming_opcodes\">\n"
	" [cannot display chart]\n"
	" </div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"server/counters[@type=&quot;opcode&quot;]/counter[. &gt; 0 or substring(@name,1,3) != 'RES']\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <tr>\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" <tr>\n"
	" <th class=\"totals\">Total:</th>\n"
	" <td class=\"totals\">\n"
	" <xsl:value-of select=\"sum(server/counters[@type=&quot;opcode&quot;]/counter)\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"server/counters[@type=&quot;qtype&quot;]/counter\">\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <h3>Incoming Queries by Query Type</h3>\n"
	" <div class=\"pie\" id=\"chart_incoming_qtypes\">\n"
	" [cannot display chart]\n"
	" </div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"server/counters[@type=&quot;qtype&quot;]/counter\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" <tr>\n"
	" <th class=\"totals\">Total:</th>\n"
	" <td class=\"totals\">\n"
	" <xsl:value-of select=\"sum(server/counters[@type=&quot;qtype&quot;]/counter)\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"views/view[count(counters[@type=&quot;resqtype&quot;]/counter) &gt; 0]\">\n"
	" <h2>Outgoing Queries per view</h2>\n"
	" <xsl:for-each select=\"views/view[count(counters[@type=&quot;resqtype&quot;]/counter) &gt; 0]\">\n"
	" <h3>View <xsl:value-of select=\"@name\"/></h3>\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <script type=\"text/javascript\">\n"
	" graphs.push({\n"
	" 'title': \"Outgoing Queries for view: <xsl:value-of select=\"@name\"/>\",\n"
	" 'target': 'chart_outgoing_queries_view_<xsl:value-of select=\"@name\"/>',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Type','Counter'],<xsl:for-each select=\"counters[@type=&quot;resqtype&quot;]/counter\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]\n"
	" });\n"
	" </script>\n"
	" <xsl:variable name=\"target\">\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </xsl:variable>\n"
	" <div class=\"pie\" id=\"chart_outgoing_queries_view_{$target}\">[no data to display]</div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"counters[@type=&quot;resqtype&quot;]/counter\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class1\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class1}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:for-each>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"server/counters[@type=&quot;nsstat&quot;]/counter[.&gt;0]\">\n"
	" <h2>Server Statistics</h2>\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <script type=\"text/javascript\">\n"
	" graphs.push({\n"
	" 'title' : \"Server Counters\",\n"
	" 'target': 'chart_server_nsstat_restype',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Type','Counter'],<xsl:for-each select=\"server/counters[@type=&quot;nsstat&quot;]/counter[.&gt;0]\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]\n"
	" });\n"
	" </script>\n"
	" <div class=\"pie\" id=\"chart_server_nsstat_restype\">[no data to display]</div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"server/counters[@type=&quot;nsstat&quot;]/counter[.&gt;0]\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class2\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class2}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"server/counters[@type=&quot;zonestat&quot;]/counter[.&gt;0]\">\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <h2>Zone Maintenance Statistics</h2>\n"
	" <script type=\"text/javascript\">\n"
	" graphs.push({\n"
	" 'title' : \"Zone Maintenance Stats\",\n"
	" 'target': 'chart_server_zone_maint',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Type','Counter'],<xsl:for-each select=\"server/counters[@type=&quot;zonestat&quot;]/counter[.&gt;0]\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]\n"
	" });\n"
	" </script>\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <div class=\"pie\" id=\"chart_server_zone_maint\">[no data to display]</div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"server/counters[@type=&quot;zonestat&quot;]/counter\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class3\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class3}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"server/counters[@type=&quot;resstat&quot;]/counter[.&gt;0]\">\n"
	" <h2>Resolver Statistics (Common)</h2>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"server/counters[@type=&quot;resstat&quot;]/counter\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class4\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class4}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" </xsl:if>\n"
	" <xsl:for-each select=\"views/view\">\n"
	" <xsl:if test=\"counters[@type=&quot;resstats&quot;]/counter[.&gt;0]\">\n"
	" <h3>Resolver Statistics for View <xsl:value-of select=\"@name\"/></h3>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"counters[@type=&quot;resstats&quot;]/counter[.&gt;0]\">\n"
	" <xsl:sort select=\".\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class5\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class5}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" </xsl:if>\n"
	" </xsl:for-each>\n"
	"\n"
	" <xsl:for-each select=\"views/view\">\n"
	" <xsl:if test=\"cache/rrset\">\n"
	" <h3>Cache DB RRsets for View <xsl:value-of select=\"@name\"/></h3>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"cache/rrset\">\n"
	" <xsl:variable name=\"css-class6\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class6}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\"counter\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" </xsl:for-each>\n"
	"\n"
	" <xsl:if test=\"server/counters[@type=&quot;sockstat&quot;]/counter[.&gt;0]\">\n"
	" <h2>Socket I/O Statistics</h2>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"server/counters[@type=&quot;sockstat&quot;]/counter[.&gt;0]\">\n"
	" <xsl:variable name=\"css-class7\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class7}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"views/view[zones/zone/counters[@type=&quot;rcode&quot;]/counter &gt;0]\">\n"
	" <h2>Response Codes per view/zone</h2>\n"
	" <xsl:for-each select=\"views/view[zones/zone/counters[@type=&quot;rcode&quot;]/counter &gt;0]\">\n"
	" <h3>View <xsl:value-of select=\"@name\"/></h3>\n"
	" <xsl:variable name=\"thisview\">\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </xsl:variable>\n"
	" <xsl:for-each select=\"zones/zone\">\n"
	" <xsl:if test=\"counters[@type=&quot;rcode&quot;]/counter[. &gt; 0]\">\n"
	" <h4>Zone <xsl:value-of select=\"@name\"/></h4>\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <script type=\"text/javascript\">\n"
	" graphs.push({\n"
	" 'title': \"Response Codes for zone <xsl:value-of select=\"@name\"/>\",\n"
	" 'target': 'chart_rescode_<xsl:value-of select=\"../../@name\"/>_<xsl:value-of select=\"@name\"/>',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Type','Counter'],<xsl:for-each select=\"counters[@type=&quot;rcode&quot;]/counter[.&gt;0 and @name != &quot;QryAuthAns&quot;]\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]\n"
	" });\n"
	"\n"
	" </script>\n"
	" <xsl:variable name=\"target\">\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </xsl:variable>\n"
	" <div class=\"pie\" id=\"chart_rescode_{$thisview}_{$target}\">[no data to display]</div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"counters[@type=&quot;rcode&quot;]/counter[.&gt;0 and @name != &quot;QryAuthAns&quot;]\">\n"
	" <xsl:sort select=\".\"/>\n"
	" <xsl:variable name=\"css-class10\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class10}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" </xsl:if>\n"
	" </xsl:for-each>\n"
	" </xsl:for-each>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"views/view[zones/zone/counters[@type=&quot;qtype&quot;]/counter &gt;0]\">\n"
	" <h2>Received QTYPES per view/zone</h2>\n"
	" <xsl:for-each select=\"views/view[zones/zone/counters[@type=&quot;qtype&quot;]/counter &gt;0]\">\n"
	" <h3>View <xsl:value-of select=\"@name\"/></h3>\n"
	" <xsl:variable name=\"thisview2\">\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </xsl:variable>\n"
	" <xsl:for-each select=\"zones/zone\">\n"
	" <xsl:if test=\"counters[@type=&quot;qtype&quot;]/counter[count(.) &gt; 0]\">\n"
	" <h4>Zone <xsl:value-of select=\"@name\"/></h4>\n"
	" <xsl:if test=\"system-property('xsl:vendor')!='Transformiix'\">\n"
	" <!-- Non Mozilla specific markup -->\n"
	" <script type=\"text/javascript\">\n"
	" graphs.push({\n"
	" 'title': \"Query Types for zone <xsl:value-of select=\"@name\"/>\",\n"
	" 'target': 'chart_qtype_<xsl:value-of select=\"../../@name\"/>_<xsl:value-of select=\"@name\"/>',\n"
	" 'style': 'barchart',\n"
	" 'data': [['Type','Counter'],<xsl:for-each select=\"counters[@type=&quot;qtype&quot;]/counter[.&gt;0 and @name != &quot;QryAuthAns&quot;]\">['<xsl:value-of select=\"@name\"/>',<xsl:value-of select=\".\"/>],</xsl:for-each>]\n"
	" });\n"
	"\n"
	" </script>\n"
	" <xsl:variable name=\"target\">\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </xsl:variable>\n"
	" <div class=\"pie\" id=\"chart_qtype_{$thisview2}_{$target}\">[no data to display]</div>\n"
	" </xsl:if>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"counters[@type=&quot;qtype&quot;]/counter\">\n"
	" <xsl:sort select=\".\"/>\n"
	" <xsl:variable name=\"css-class11\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class11}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"@name\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" </xsl:if>\n"
	" </xsl:for-each>\n"
	" </xsl:for-each>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"socketmgr/sockets/socket\">\n"
	" <h2>Network Status</h2>\n"
	" <table class=\"netstat\">\n"
	" <tr>\n"
	" <th>ID</th>\n"
	" <th>Name</th>\n"
	" <th>Type</th>\n"
	" <th>References</th>\n"
	" <th>LocalAddress</th>\n"
	" <th>PeerAddress</th>\n"
	" <th>State</th>\n"
	" </tr>\n"
	" <xsl:for-each select=\"socketmgr/sockets/socket\">\n"
	" <xsl:sort select=\"id\"/>\n"
	" <xsl:variable name=\"css-class12\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class12}\">\n"
	" <td>\n"
	" <xsl:value-of select=\"id\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"name\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"type\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"references\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"local-address\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"peer-address\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:for-each select=\"states\">\n"
	" <xsl:value-of select=\".\"/>\n"
	" </xsl:for-each>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"taskmgr/thread-model/type\">\n"
	" <h2>Task Manager Configuration</h2>\n"
	" <table class=\"counters\">\n"
	" <tr>\n"
	" <th class=\"even\">Thread-Model</th>\n"
	" <td>\n"
	" <xsl:value-of select=\"taskmgr/thread-model/type\"/>\n"
	" </td>\n"
	" </tr>\n"
	" <tr class=\"odd\">\n"
	" <th>Worker Threads</th>\n"
	" <td>\n"
	" <xsl:value-of select=\"taskmgr/thread-model/worker-threads\"/>\n"
	" </td>\n"
	" </tr>\n"
	" <tr class=\"even\">\n"
	" <th>Default Quantum</th>\n"
	" <td>\n"
	" <xsl:value-of select=\"taskmgr/thread-model/default-quantum\"/>\n"
	" </td>\n"
	" </tr>\n"
	" <tr class=\"odd\">\n"
	" <th>Tasks Running</th>\n"
	" <td>\n"
	" <xsl:value-of select=\"taskmgr/thread-model/tasks-running\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"taskmgr/tasks/task\">\n"
	" <h2>Tasks</h2>\n"
	" <table class=\"tasks\">\n"
	" <tr>\n"
	" <th>ID</th>\n"
	" <th>Name</th>\n"
	" <th>References</th>\n"
	" <th>State</th>\n"
	" <th>Quantum</th>\n"
	" </tr>\n"
	" <xsl:for-each select=\"taskmgr/tasks/task\">\n"
	" <xsl:sort select=\"name\"/>\n"
	" <xsl:variable name=\"css-class14\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class14}\">\n"
	" <td>\n"
	" <xsl:value-of select=\"id\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"name\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"references\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"state\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"quantum\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"memory/summary\">\n"
	" <h2>Memory Usage Summary</h2>\n"
	" <table class=\"counters\">\n"
	" <xsl:for-each select=\"memory/summary/*\">\n"
	" <xsl:variable name=\"css-class13\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class13}\">\n"
	" <th>\n"
	" <xsl:value-of select=\"name()\"/>\n"
	" </th>\n"
	" <td>\n"
	" <xsl:value-of select=\".\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" <br/>\n"
	" </xsl:if>\n"
	" <xsl:if test=\"memory/contexts/context\">\n"
	" <h2>Memory Contexts</h2>\n"
	" <table class=\"mctx\">\n"
	" <tr>\n"
	" <th>ID</th>\n"
	" <th>Name</th>\n"
	" <th>References</th>\n"
	" <th>TotalUse</th>\n"
	" <th>InUse</th>\n"
	" <th>MaxUse</th>\n"
	" <th>BlockSize</th>\n"
	" <th>Pools</th>\n"
	" <th>HiWater</th>\n"
	" <th>LoWater</th>\n"
	" </tr>\n"
	" <xsl:for-each select=\"memory/contexts/context\">\n"
	" <xsl:sort select=\"total\" data-type=\"number\" order=\"descending\"/>\n"
	" <xsl:variable name=\"css-class14\">\n"
	" <xsl:choose>\n"
	" <xsl:when test=\"position() mod 2 = 0\">even</xsl:when>\n"
	" <xsl:otherwise>odd</xsl:otherwise>\n"
	" </xsl:choose>\n"
	" </xsl:variable>\n"
	" <tr class=\"{$css-class14}\">\n"
	" <td>\n"
	" <xsl:value-of select=\"id\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"name\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"references\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"total\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"inuse\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"maxinuse\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"blocksize\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"pools\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"hiwater\"/>\n"
	" </td>\n"
	" <td>\n"
	" <xsl:value-of select=\"lowater\"/>\n"
	" </td>\n"
	" </tr>\n"
	" </xsl:for-each>\n"
	" </table>\n"
	" </xsl:if>\n"
	" <hr/>\n"
	" <p class=\"footer\">Internet Systems Consortium Inc.<br/><a href=\"http://www.isc.org\">http://www.isc.org</a></p>\n"
	" </body>\n"
	" </html>\n"
	" </xsl:template>\n"
	"</xsl:stylesheet>\n";
