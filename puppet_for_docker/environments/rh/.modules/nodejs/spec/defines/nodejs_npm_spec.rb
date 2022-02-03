require 'spec_helper'

describe 'nodejs::npm', type: :define do
  let :pre_condition do
    'class { "nodejs": }'
  end

  on_supported_os.each do |os, facts|
    context "on #{os} " do
      let :facts do
        facts
      end

      # This should only affect the exec title
      let(:title) { 'express' }

      # su user npm install <package>
      context 'with package set to express, user set to foo and target set to /home/npm/packages' do
        let :params do
          {
            package: 'express',
            target: '/home/npm/packages',
            user: 'foo'
          }
        end

        it 'the npm install command should run under user foo' do
          is_expected.to contain_exec('npm_install_express').with(
            'command' => '/usr/bin/npm install express ',
            'user'    => 'foo'
          )
        end
      end

      # npm install <package>
      context 'with package set to express and target set to /home/npm/packages' do
        let :params do
          {
            package: 'express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install express ')
        end

        it 'the install_check should grep for /home/npm/packages/node_modules/express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/express"')
        end

        it 'the exec directory should be /home/npm/packages' do
          is_expected.to contain_exec('npm_install_express').with('cwd' => '/home/npm/packages')
        end

        it 'the environment variable HOME should be /root' do
          is_expected.to contain_exec('npm_install_express').with('environment' => 'HOME=/root')
        end
      end

      # npm install <package> <install_options>
      context "with package set to express and install_options set to ['--no-bin-links', '--no-optional']" do
        let :params do
          {
            install_options: ['--no-bin-links', '--no-optional'],
            package: 'express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install express --no-bin-links --no-optional' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install express --no-bin-links --no-optional')
        end
      end

      # npm install <@scope/package>
      context 'with package set to @scopename/express' do
        let :params do
          {
            package: '@scopename/express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install @scopename/express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install @scopename/express ')
        end

        it 'the install_check should grep for /home/npm/packages/node_modules/@scopename/express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/@scopename/express"')
        end
      end

      # npm install <@scope/package@tag>
      context 'with package set to @scopename/express and ensure set to tagname' do
        let :params do
          {
            ensure: 'tagname',
            package: '@scopename/express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install @scopename/express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install @scopename/express@tagname ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/@scopename/express:@scopename/express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/@scopename/express:@scopename/express@tagname"')
        end
      end

      # npm install <package@tag>
      context 'with package set to express and ensure set to tagname' do
        let :params do
          {
            ensure: 'tagname',
            package: 'express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install express@tagname ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/express:express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/express:express@tagname"')
        end
      end

      # npm install <@scope/package@version>
      context 'with package set to @scopename/express and ensure set to 3' do
        let :params do
          {
            ensure: '3',
            package: '@scopename/express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install @scopename/express@3' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install @scopename/express@3 ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/@scopename/express:@scopename/express@3' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/@scopename/express:@scopename/express@3"')
        end
      end

      # npm install <package@version>
      context 'with package set to express and ensure set to 3' do
        let :params do
          {
            ensure: '3',
            package: 'express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install express@3' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install express@3 ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/express:express@3' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/express:express@3"')
        end
      end

      # npm install <folder>
      context 'with package set to express and source set to /local/folder' do
        let :params do
          {
            package: 'express',
            source: '/local/folder',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install /local/folder' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install /local/folder ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules//local/folder' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules//local/folder"')
        end
      end

      # npm install <local tarball file>
      context 'with package set to express and source set to /local/package.tgz' do
        let :params do
          {
            package: 'express',
            source: '/local/package.tgz',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install /local/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install /local/package.tgz ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules//local/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules//local/package.tgz"')
        end
      end

      # npm install <http remote tarball>
      context 'with package set to express and source set to http://domain/package.tgz' do
        let :params do
          {
            package: 'express',
            source: 'http://domain/package.tgz',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install http://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install http://domain/package.tgz ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/http://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/http://domain/package.tgz"')
        end
      end

      # npm install <https remote tarball>
      context 'with package set to express and source set to https://domain/package.tgz' do
        let :params do
          {
            package: 'express',
            source: 'https://domain/package.tgz',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install https://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install https://domain/package.tgz ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/https://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/https://domain/package.tgz"')
        end
      end

      # npm install <strongloop/express>
      context 'with package set to express and source set to strongloop/express' do
        let :params do
          {
            package: 'express',
            source: 'strongloop/express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install strongloop/express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install strongloop/express ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/strongloop/express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/strongloop/express"')
        end
      end

      # npm install <git URL>
      context 'with package set to express and source set to git://github.com/strongloop/expressjs.git' do
        let :params do
          {
            package: 'express',
            source: 'git://github.com/strongloop/expressjs.git',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install git://github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install git://github.com/strongloop/expressjs.git ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/git://github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/git://github.com/strongloop/expressjs.git"')
        end
      end

      # npm install <git+ssh URL>
      context 'with package set to express and source set to git+ssh://git@github.com:/strongloop/expressjs.git' do
        let :params do
          {
            package: 'express',
            source: 'git+ssh://git@github.com:/strongloop/expressjs.git',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install git+ssh://git@github.com:/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install git+ssh://git@github.com:/strongloop/expressjs.git ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/git+ssh://git@github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/git+ssh://git@github.com:/strongloop/expressjs.git"')
        end
      end

      # npm install <git+https URL>
      context 'with package set to express and source set to git+https://user@github.com/strongloop/expressjs.git' do
        let :params do
          {
            package: 'express',
            source: 'git+https://user@github.com/strongloop/expressjs.git',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm install git+https://user@github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install git+https://user@github.com/strongloop/expressjs.git ')
        end

        it 'the install_check should grep for /home/npm/package/node_modules/git+https://user@github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/git+https://user@github.com/strongloop/expressjs.git"')
        end
      end

      # npm install with package.json
      context 'with package set to a valid value, ensure set to present and use_package_json set to true' do
        let :params do
          {
            ensure: 'present',
            package: 'express',
            target: '/home/user/project',
            use_package_json: true
          }
        end

        it 'the command should be npm install' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/usr/bin/npm install ')
        end

        it 'the list_command should check if all deps are installed in /home/user/project/node_modules' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/usr/bin/npm ls --long --parseable')
        end
      end

      # npm rm <package>
      context 'with package set to express and ensure set to absent' do
        let :params do
          {
            ensure: 'absent',
            package: 'express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be npm rm express' do
          is_expected.to contain_exec('npm_rm_express').with('command' => '/usr/bin/npm rm express ')
        end

        it 'the install_check should grep for /home/npm/packages/node_modules/express' do
          is_expected.to contain_exec('npm_rm_express').with('onlyif'  => '/usr/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/express"')
        end
      end

      # npm rm <package> <uninstall_options>
      context "with package set to express, ensure set to absent and uninstall_options set to ['--save']" do
        let :params do
          {
            ensure: 'absent',
            package: 'express',
            target: '/home/npm/packages',
            uninstall_options: ['--save']
          }
        end

        it 'the command should be npm rm express --save' do
          is_expected.to contain_exec('npm_rm_express').with(
            'command' => '/usr/bin/npm rm express --save',
            'cwd'     => '/home/npm/packages'
          )
        end
      end

      # npm uninstall with package.json
      context 'with package set to express, ensure set to absent and use_package_json set to true' do
        let :params do
          {
            ensure: 'absent',
            package: 'express',
            target: '/home/user/project',
            use_package_json: true
          }
        end

        it 'the command should be npm rm * in subdirectory node_modules' do
          is_expected.to contain_exec('npm_rm_express').with('command' => '/usr/bin/npm rm * ')
          is_expected.to contain_exec('npm_rm_express').with('cwd' => '/home/user/project/node_modules')
        end

        it 'the list_command should check if modules are installed in /home/user/project/node_modules' do
          is_expected.to contain_exec('npm_rm_express').with('onlyif' => '/usr/bin/npm ls --long --parseable')
          is_expected.to contain_exec('npm_rm_express').with('cwd' => '/home/user/project/node_modules')
        end
      end
    end

    context 'when run on Darwin' do
      let :facts do
        {
          'os' => {
            'family' => 'Darwin',
            'name' => 'Darwin'
          }
        }
      end

      # This should only affect the exec title
      let(:title) { 'express' }

      # npm install <npm_package>
      context 'with package set to express and target set to /home/npm/packages' do
        let :params do
          {
            package: 'express',
            target: '/home/npm/packages'
          }
        end

        it 'the command should be /opt/local/bin/npm install express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '/opt/local/bin/npm install express ')
        end

        it 'the install_check should grep for /home/npm/packages/node_modules/express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '/opt/local/bin/npm ls --long --parseable | grep "/home/npm/packages/node_modules/express"')
        end
      end
    end

    context 'when run on Windows' do
      let :facts do
        {
          'os' => {
            'family' => 'Windows',
            'name' => 'Windows',
            'windows' => {
              'system32' => 'C:\Windows\system32'
            }
          }
        }
      end

      # This should only affect the exec title
      let(:title) { 'express' }

      # npm install <package>
      context 'with package set to express and target set to C:\packages' do
        let :params do
          {
            package: 'express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install express ')
        end

        it 'the install_check should do findstr /l C:\packages\node_modules\express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\express"')
        end

        it 'the exec directory should be C:\packages' do
          is_expected.to contain_exec('npm_install_express').with('cwd' => 'C:\packages')
        end
      end

      # npm install <package> <install_options>
      context "with package set to express and install_options set to ['--no-bin-links', '--no-optional']" do
        let :params do
          {
            package: 'express',
            target: 'C:\packages',
            install_options: ['--no-bin-links', '--no-optional']
          }
        end

        it 'the command should be npm install express --no-bin-links --no-optional' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install express --no-bin-links --no-optional')
        end
      end

      # npm install <@scope/package>
      context 'with package set to @scopename/express' do
        let :params do
          {
            package: '@scopename/express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install @scopename/express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install @scopename/express ')
        end

        it 'the install_check should do findstr /l C:\packages\node_modules\@scopename/express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\@scopename/express"')
        end
      end

      # npm install <@scope/package@tag>
      context 'with package set to @scopename/express and ensure set to tagname' do
        let :params do
          {
            ensure: 'tagname',
            package: '@scopename/express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install @scopename/express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install @scopename/express@tagname ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\@scopename/express:@scopename/express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\@scopename/express:@scopename/express@tagname"')
        end
      end

      # npm install <npm_package@tag>
      context 'with package set to express and ensure set to tagname' do
        let :params do
          {
            ensure: 'tagname',
            package: 'express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install express@tagname ')
        end

        it 'the install_check should findstr /l for C:\packages\node_modules\express:express@tagname' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\express:express@tagname"')
        end
      end

      # npm install <@scope/package@version>
      context 'with package set to @scopename/express and ensure set to 3' do
        let :params do
          {
            ensure: '3',
            package: '@scopename/express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install @scopename/express@3' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install @scopename/express@3 ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\@scopename/express:@scopename/express@3' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\@scopename/express:@scopename/express@3"')
        end
      end

      # npm install <package@3>
      context 'with package set to express and ensure set to 3' do
        let :params do
          {
            ensure: '3',
            package: 'express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install express@3' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install express@3 ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\express:express@3' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\express:express@3"')
        end
      end

      # npm install <folder>
      context 'with package set to express and source set to C:\local\folder' do
        let :params do
          {
            package: 'express',
            source: 'C:\local\folder',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install C:\local\folder' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install C:\local\folder ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\local\folder' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\C:\local\folder"')
        end
      end

      # npm install <local tarball file>
      context 'with package set to express and source set to C:\local\package.tgz' do
        let :params do
          {
            package: 'express',
            source: 'C:\local\package.tgz',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install C:\local\package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install C:\local\package.tgz ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\local\package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\C:\local\package.tgz"')
        end
      end

      # npm install <http remote tarball>
      context 'with package set to express and source set to http://domain/package.tgz' do
        let :params do
          {
            package: 'express',
            source: 'http://domain/package.tgz',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install http://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install http://domain/package.tgz ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\http://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\http://domain/package.tgz"')
        end
      end

      # npm install <https remote tarball>
      context 'with package set to express and source set to https://domain/package.tgz' do
        let :params do
          {
            package: 'express',
            source: 'https://domain/package.tgz',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install https://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install https://domain/package.tgz ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\https://domain/package.tgz' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\https://domain/package.tgz"')
        end
      end

      # npm install <strongloop/express>
      context 'with package set to express and source set to strongloop/express' do
        let :params do
          {
            package: 'express',
            source: 'strongloop/express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install strongloop/express' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install strongloop/express ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\strongloop/express' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\strongloop/express"')
        end
      end

      # npm install <git URL>
      context 'with package set to express and source set to git://github.com/strongloop/expressjs.git' do
        let :params do
          {
            package: 'express',
            source: 'git://github.com/strongloop/expressjs.git',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install git://github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install git://github.com/strongloop/expressjs.git ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\git://github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\git://github.com/strongloop/expressjs.git"')
        end
      end

      # npm install <git+ssh URL>
      context 'with package set to express and source set to git+ssh://git@github.com:/strongloop/expressjs.git' do
        let :params do
          {
            package: 'express',
            source: 'git+ssh://git@github.com:/strongloop/expressjs.git',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install git+ssh://git@github.com:/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install git+ssh://git@github.com:/strongloop/expressjs.git ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\git+ssh://git@github.com:/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\git+ssh://git@github.com:/strongloop/expressjs.git"')
        end
      end

      # npm install <git+https URL>
      context 'package set to express and source set to git+https://user@github.com/strongloop/expressjs.git' do
        let :params do
          {
            package: 'express',
            source: 'git+https://user@github.com/strongloop/expressjs.git',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm install git+https://user@github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install git+https://user@github.com/strongloop/expressjs.git ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\git+https://user@github.com/strongloop/expressjs.git' do
          is_expected.to contain_exec('npm_install_express').with('unless'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\git+https://user@github.com/strongloop/expressjs.git"')
        end
      end

      # npm install with package.json
      context 'with package set to a valid value, ensure set to present and use_package_json set to true' do
        let :params do
          {
            ensure: 'present',
            package: 'express',
            target: 'C:\Users\test\project',
            use_package_json: true
          }
        end

        it 'the command should be npm install' do
          is_expected.to contain_exec('npm_install_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" install ')
        end

        it 'the list_command should check if all deps are installed in /home/user/project/node_modules' do
          is_expected.to contain_exec('npm_install_express').with('unless' => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable')
        end
      end

      # npm rm <package>
      context 'with package set to express and ensure set to absent' do
        let :params do
          {
            ensure: 'absent',
            package: 'express',
            target: 'C:\packages'
          }
        end

        it 'the command should be npm rm express' do
          is_expected.to contain_exec('npm_rm_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" rm express ')
        end

        it 'the install_check should findstr /l C:\packages\node_modules\express' do
          is_expected.to contain_exec('npm_rm_express').with('onlyif'  => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable | C:\Windows\system32\cmd.exe /c findstr /l "C:\packages\node_modules\express"')
        end
      end

      # npm rm <package> <uninstall_options>
      context "with package set to express, ensure set to absent and uninstall_options set to ['--save']" do
        let :params do
          {
            ensure: 'absent',
            package: 'express',
            target: 'C:\packages',
            uninstall_options: ['--save']
          }
        end

        it 'the command should be npm rm express --save' do
          is_expected.to contain_exec('npm_rm_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" rm express --save')
        end
      end

      # npm uninstall with package.json
      context 'with package set to express, ensure set to absent and use_package_json set to true' do
        let :params do
          {
            ensure: 'absent',
            package: 'express',
            target: 'C:\Users\test\project',
            use_package_json: true
          }
        end

        it 'the command should be npm rm * in subdirectory node_modules' do
          is_expected.to contain_exec('npm_rm_express').with('command' => '"C:\Program Files\nodejs\npm.cmd" rm * ')
          is_expected.to contain_exec('npm_rm_express').with('cwd' => 'C:\Users\test\project\node_modules')
        end

        it 'the list_command should check if modules are installed in C:\Users\test\project\node_modules' do
          is_expected.to contain_exec('npm_rm_express').with('onlyif' => '"C:\Program Files\nodejs\npm.cmd" ls --long --parseable')
          is_expected.to contain_exec('npm_rm_express').with('cwd' => 'C:\Users\test\project\node_modules')
        end
      end
    end
  end
end
