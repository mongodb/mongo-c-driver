<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
                version="1.0">
    <!--
    Turn markup like this:

    <code xref="bson:bson_t">bson_t</code>

    ... into a link like this:

    http://api.mongodb.org/libbson/current/bson_t.html
    -->
    <xsl:template name="mal.link.target.custom">
        <xsl:param name="node" select="."/>
        <xsl:param name="xref" select="$node/@xref"/>
        <xsl:if test="starts-with($xref, 'bson:')">
            <xsl:variable name="ref"
                          select="substring-after($xref, 'bson:')"/>
            <xsl:text>http://mongoc.org/libbson/current/</xsl:text>
            <xsl:value-of select="$ref"/>
            <xsl:text>.html</xsl:text>
        </xsl:if>
    </xsl:template>

    <xsl:template name="html.head.custom">
        <xsl:text disable-output-escaping="yes"><![CDATA[
            <style type="text/css">
              .section-anchor {
                  display: block;
                  opacity: 0;
                  float: left;
                  margin-left: -0.8em;
                  padding-right: 0.2em;
                  margin-top: 0.2em;
                  cursor: pointer;
              }

              .section-anchor:hover {
                  opacity: 1;
              }

              .footer {
                  float: right;
                  padding: 1em 0;
                  font-style: italic;
              }
            </style>
            <script type="text/javascript">
              document.addEventListener("DOMContentLoaded", function() {
                  // For each section.
                  var elems = document.querySelectorAll('.sect');
                  for (var i = 0; i < elems.length; i++) {
                      // Make a closure to remember what "a" is.
                      (function() {
                          var section = elems[i];
                          if (!section.id) {
                            return;
                          }

                          var a = document.createElement('a');
                          a.href = '#' + section.id;
                          a.innerHTML = '&sect;';
                          a.classList.add('section-anchor');
                          section.insertBefore(a, section.firstChild);

                          var header = section.querySelector('h1, h2, h3, h4');
                          var mouseEnter = function() {
                              a.style.opacity = 1;
                          };

                          var mouseLeave = function() {
                              a.style.opacity = 0;
                          };

                          header.addEventListener("mouseenter", mouseEnter);
                          header.addEventListener("mouseleave", mouseLeave);
                          a.addEventListener("mouseenter", mouseEnter);
                          a.addEventListener("mouseleave", mouseLeave);
                      })();
                  };
              });
            </script>]]>
        </xsl:text>
    </xsl:template>

    <xsl:param name="libversion" select="document('version.xml')"/>

    <xsl:template name="html.footer.custom">
        <section id="version">
            <xsl:value-of select="$libversion" />
        </section>
    </xsl:template>

</xsl:stylesheet>
