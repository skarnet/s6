#!/command/execlineb -S0

# For dev use only. Don't run this, it overwrites your configure.

# The quoting interactions in sed and sh make it impossible to get
# such a simple thing done. It's amazing how bad traditional Unix is.

backtick -E TEMPLATE { redirfd -r 0 tools/configure.template s6-cat }
s6-envdir -Lf package/configure-snippets
multisubstitute
{
  importas -uS configure_help_install
  importas -uS configure_help_dependencies
  importas -uS configure_help_options
  importas -uS configure_init_vars
  importas -uS configure_case_lines
  importas -uS configure_expand_dirs
  importas -uS configure_slashpackage_other
  importas -uS configure_extra_checks
  importas -uS configure_generate_make
  importas -uS configure_generate_configh
}

if
{
  heredoc 0 ${TEMPLATE}
  redirfd -w 1 configure.new
  s6-cat
}

if { s6-chmod 0755 configure.new }
s6-rename configure.new configure
