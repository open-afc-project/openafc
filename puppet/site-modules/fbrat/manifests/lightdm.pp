# LightDM packages from CentOS-7 EPEL
class fbrat::lightdm {
  ensure_packages(
    [
      'xorg-x11-server-Xorg',
      'xorg-x11-drv-keyboard',
      'xorg-x11-drv-mouse',
      'xorg-x11-drv-evdev',
      'xorg-x11-drv-libinput',
      'xorg-x11-drv-vmmouse',
      'xorg-x11-drv-dummy',
      'xorg-x11-drv-fbdev',
      'xorg-x11-drv-vesa',
      'xorg-x11-xinit',
      'xorg-x11-xinit-session',
      'xorg-x11-xkb-utils',
      'mesa-dri-drivers',
      'lightdm',
    ],
    {
      ensure => 'installed',
      install_options => ['--enablerepo', 'epel'],
    }
  )
  service { 'lightdm':
    enable  => true,
    require => Package['lightdm'],
  }
}
