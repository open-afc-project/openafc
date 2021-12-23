# XFCE packages from CentOS-7 EPEL
class fbrat::xfce_desktop {
  ensure_packages(
    [
      'xfce4-session',
      'xfdesktop',
      'xfce4-settings',
      'xfce4-terminal',
      # GUI data
      'dejavu-serif-fonts',
      'dejavu-sans-fonts',
      'dejavu-sans-mono-fonts',
      'adwaita-gtk2-theme',
      'adwaita-icon-theme',
    ],
    {
      ensure => 'installed',
      install_options => ['--enablerepo', 'epel'],
    }
  )
  file { '/etc/sysconfig/desktop':
    content => "PREFERRED=xfce4-session\n",
  }
}
