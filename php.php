<?php
$tests = array();
$depends = array();
$tasks = array();
$version = array("2.4", "2.6", "3.0", "3.2", "latest");
$topologies = array("server", "replica_set", "sharded_cluster" /*, "sharded_cluster_multi_mongos"*/);
$auth = array("noauth", "auth");
$ssl = array("nossl", "openssl", "darwinssl", "winssl");
$sasl = array("nosasl", "sasl");

function t($AUTH, $VERSION, $TOPOLOGY, $SSL, $SASL) {
   global $tests;
   global $depends;
   global $tasks;

   if ($AUTH == "auth" && $SSL == "nossl") {
      return;
   }
   $URI="";
   $extraname="";
   if ($TOPOLOGY == "sharded_cluster_multi_mongos") {
      $TOPOLOGY = "sharded_cluster";
      $URI="mongodb://localhost:27017,localhost:27018";
      $extraname="-multi-mongos";
   }
   $t = str_replace("_", "-", $TOPOLOGY);
   $tests[] = $test = "- name: test-$VERSION-$t{$extraname}-$AUTH-$SASL-$SSL";
   $depend = "debug-compile-$SASL-$SSL";
   $depends[$depend] = array($SASL, $SSL);
   $tasks[] = <<< EOD
    $test
      tags: ["$SSL", "$SASL", "$AUTH", "$t", "$VERSION"]
      depends_on:
        - name: "$depend"
      commands:
        - func: "fetch build"
          vars:
            BUILD_NAME: "$depend"
        - func: "bootstrap mongo-orchestration"
          vars:
            VERSION: "$VERSION"
            TOPOLOGY: "$TOPOLOGY"
            AUTH: "$AUTH"
            SSL: "$SSL"
        - func: "run tests"
          vars:
            AUTH: "$AUTH"
            SSL: "$SSL"
            URI: "$URI"



EOD;

}

foreach($version as $v) {
   foreach($topologies as $t) {
      foreach ($auth as $a) {
         foreach($sasl as $sa) {
            foreach($ssl as $s) {
               t($a, $v, $t, $s, $sa);
            }
         }
      }
   }
}
foreach($tests as $t) {
   //echo $t, "\n";
}
echo "\n";

echo <<< EOMATRIX
# Compile Matrix {{{
    - name: make-release-archive
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              sudo apt-get install -y awscli
              ./autogen.sh --enable-html-docs --enable-man-pages && make dist
        - func: "upload docs"
        - func: "upload release"
        - func: "upload build"

    - name: debug-compile
      tags: ["debug-compile"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' sh .evergreen/compile.sh
        - func: "upload build"

    - name: debug-compile-c11
      tags: ["debug-compile", "special", "c11", "stdflags"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' CFLAGS='-std=c11 -D_XOPEN_SOURCE=600' sh .evergreen/compile.sh
        - func: "upload build"

    - name: debug-compile-c99
      tags: ["debug-compile", "special", "c99", "stdflags"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' CFLAGS='-std=c99 -D_XOPEN_SOURCE=600' sh .evergreen/compile.sh
        - func: "upload build"

    - name: debug-compile-c89
      tags: ["debug-compile", "special", "c89", "stdflags"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
               DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' CFLAGS='-std=c89 -D_POSIX_C_SOURCE=200112L' sh .evergreen/compile.sh
        - func: "upload build"

    - name: debug-compile-valgrind
      tags: ["debug-compile", "special", "valgrind"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              VALGRIND=yes DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' sh .evergreen/compile.sh
        - func: "upload build"

    - name: debug-compile-sanitizer-address
      tags: ["debug-compile", "special", "sanitizer", "clang"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC='clang' MARCH='\${MARCH}' LDFLAGS='-fsanitize' CFLAGS='-fsanitize=address' sh .evergreen/compile.sh
        - func: "upload build"

    - name: debug-compile-coverage
      tags: ["debug-compile", "special", "coverage"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' COVERAGE=yes sh .evergreen/compile.sh
        - func: "upload coverage"
        - func: "upload build"

    - name: debug-compile-scan-build
      tags: ["debug-compile", "special", "scan-build", "clang"]
      commands:
        - command: shell.exec
          params:
            continue_on_err: true
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC=clang ANALYZE=yes MARCH='\${MARCH}' sh .evergreen/compile.sh
        - func: "upload scan artifacts"
        - func: "upload build"
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
               if find scan -name \*.html | grep -q html; then
                  exit 123
               fi

    - name: release-compile
      tags: ["release-compile"]
      depends_on:
        - name: "make-release-archive"
          variant: releng
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              RELEASE=yes CC='\${CC}' MARCH='\${MARCH}' sh .evergreen/compile.sh
        - func: "upload build"

EOMATRIX;
foreach($depends as $t => $args) {
   $SASL = $args[0] == "nosasl" ? "no": $args[0];
   $sasltag = $args[0];
   switch($args[1]) {
   case "nossl":
      $ssltag = "nossl";
      $SSL="no";
      break;
   case "darwinssl":
      $ssltag = "darwinssl";
      $SSL="darwin";
      break;
   case "openssl":
      $ssltag = "openssl";
      $SSL="openssl";
      break;
   case "winssl":
      $ssltag = "winssl";
      $SSL="winssl";
      break;
   default:
      exit("Unknown args: $args[1]");
   }
   echo <<< EOF
    - name: $t
      tags: ["debug-compile", "$sasltag", "$ssltag"]
      commands:
        - command: shell.exec
          type: test
          params:
            working_dir: "mongoc"
            script: |
              set -o errexit
              set -o xtrace
              DEBUG=yes CC='\${CC}' MARCH='\${MARCH}' SASL=$SASL SSL=$SSL sh .evergreen/compile.sh
        - func: "upload build"


EOF;
}
echo "# }}}\n\n";

echo "# Test Matrix {{{\n";
foreach($tasks as $t) {
   echo $t;
}
echo "# }}}\n\n";
