set (input_vars
  src_dir
  bin_dir
  prefix
  includedir
  libdir
  output_name
  soname
  version
  is_static
)

foreach (var ${input_vars})
  if (NOT DEFINED "${var}")
    message (FATAL_ERROR "${var} was not set!")
  endif ()
endforeach ()

set (cflags "-I\${includedir}")

if (is_static)
  set (pkgname "libmongoac-static")
  string (APPEND cflags " -DMONGOAC_STATIC")
  set (libs "\${libdir}/lib${soname}.a")
  set (pc_output "${bin_dir}/${output_name}-static.pc")
else ()
  set (pkgname "libmongoac")
  set (libs "-L\${libdir} -l${soname}")
  set (pc_output "${bin_dir}/${output_name}.pc")
endif ()

configure_file (
  ${src_dir}/mongoac.pc.in
  ${pc_output}
  @ONLY
)
