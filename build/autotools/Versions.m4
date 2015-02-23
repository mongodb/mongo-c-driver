m4_define([mongoc_major_version], [1])
m4_define([mongoc_minor_version], [1])
m4_define([mongoc_micro_version], [1])
m4_define([mongoc_version], [mongoc_major_version.mongoc_minor_version.mongoc_micro_version])

# bump up by 1 for every micro release with no API changes, otherwise
# set to 0. after release, bump up by 1
m4_define([mongoc_interface_age], [1])
m4_define([mongoc_binary_age], [m4_eval(100 * mongoc_minor_version + mongoc_micro_version)])

m4_define([lt_current], [m4_eval(100 * mongoc_minor_version + mongoc_micro_version - mongoc_interface_age)])
m4_define([lt_revision], [mongoc_interface_age])
m4_define([lt_age], [m4_eval(mongoc_binary_age - mongoc_interface_age)])

m4_define([libbson_required_version], [1.1.0])

m4_define([sasl_required_version], [2.1.6])
