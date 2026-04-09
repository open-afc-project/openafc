import React from 'react';
import '@patternfly/react-core/dist/styles/base.css';
import { NavLink } from 'react-router-dom';
import {
  Nav,
  NavList,
  NavItem,
  Divider,
  Page,
  Masthead,
  MastheadMain,
  MastheadBrand,
  MastheadLogo,
  MastheadContent,
  MastheadToggle,
  PageSidebar,
  PageSidebarBody,
  PageToggleButton,
  SkipToContent,
  Toolbar,
  ToolbarContent,
  ToolbarItem,
  Button,
  Tooltip,
} from '@patternfly/react-core';
import { BarsIcon, MoonIcon, SunIcon } from '@patternfly/react-icons';
import '@app/app.css';
import { guiConfig } from '../Lib/RatApi';
import { AppInfo } from './AppInfo';
import { LoginAvatar } from './LoginAvatar';
import { UserContext, isAdmin, UserState, hasRole, isLoggedIn } from '../Lib/User';

interface IAppLayout {
  children: React.ReactNode;
}

const THEME_KEY = 'pf-theme';
const DARK_CLASS = 'pf-v6-theme-dark';

const initDarkMode = (): boolean => {
  const stored = localStorage.getItem(THEME_KEY);
  if (stored !== null) return stored === 'dark';
  return window.matchMedia?.('(prefers-color-scheme: dark)').matches ?? false;
};

const AppLayout: React.FunctionComponent<IAppLayout> = ({ children }) => {
  const [isNavOpen, setIsNavOpen] = React.useState(true);
  const [isMobileView, setIsMobileView] = React.useState(true);
  const [isNavOpenMobile, setIsNavOpenMobile] = React.useState(false);
  const [isDark, setIsDark] = React.useState(initDarkMode);

  React.useEffect(() => {
    document.documentElement.classList.toggle(DARK_CLASS, isDark);
  }, [isDark]);

  const toggleTheme = () => {
    setIsDark((prev) => {
      const next = !prev;
      localStorage.setItem(THEME_KEY, next ? 'dark' : 'light');
      return next;
    });
  };

  const onNavToggleMobile = () => setIsNavOpenMobile(!isNavOpenMobile);
  const onNavToggle = () => setIsNavOpen(!isNavOpen);
  const onPageResize = (_event: any, props: { mobileView: boolean; windowSize: number }) => {
    setIsMobileView(props.mobileView);
  };

  const navLinkClassName = ({ isActive }: { isActive: boolean }) => (isActive ? 'pf-m-current' : '');

  const uls: any = (
    <a target="_blank" rel="noopener noreferrer" href={guiConfig.uls_url}>
      ULS Databases
    </a>
  );
  const antenna: any = (
    <a target="_blank" rel="noopener noreferrer" href={guiConfig.antenna_url}>
      Antenna Patterns
    </a>
  );
  const history: any = (
    <a target="_blank" rel="noopener noreferrer" href={guiConfig.history_url}>
      Debug Files
    </a>
  );

  const showAbout = () => guiConfig.about_url;

  const topNavLinkStyle: React.CSSProperties = {
    color: 'var(--pf-t--global--text--color--regular, #000)',
    textDecoration: 'none',
    padding: '0 8px',
    whiteSpace: 'nowrap',
  };

  const headerToolbar = (
    <Toolbar id="header-toolbar" className="pf-m-align-items-center">
      <ToolbarContent className="pf-m-align-items-center">
        <UserContext.Consumer>
          {(u: UserState) =>
            isLoggedIn() ? (
              <ToolbarItem>
                <NavLink to="/account" className={navLinkClassName} style={topNavLinkStyle}>
                  Account
                </NavLink>
              </ToolbarItem>
            ) : null
          }
        </UserContext.Consumer>
        <UserContext.Consumer>
          {(u: UserState) =>
            isAdmin() ? (
              <ToolbarItem>
                <NavLink to="/admin" className={navLinkClassName} style={topNavLinkStyle}>
                  Administrator
                </NavLink>
              </ToolbarItem>
            ) : null
          }
        </UserContext.Consumer>
        <UserContext.Consumer>
          {(u: UserState) =>
            isAdmin() ? (
              <ToolbarItem>
                <NavLink to="/mtls" className={navLinkClassName} style={topNavLinkStyle}>
                  MTLS
                </NavLink>
              </ToolbarItem>
            ) : null
          }
        </UserContext.Consumer>
        <UserContext.Consumer>
          {(u: UserState) =>
            isAdmin() ? (
              <ToolbarItem>
                <NavLink to="/deniedRules" className={navLinkClassName} style={topNavLinkStyle}>
                  Denied Rules
                </NavLink>
              </ToolbarItem>
            ) : null
          }
        </UserContext.Consumer>
        <ToolbarItem>
          <Tooltip content={isDark ? 'Switch to light theme' : 'Switch to dark theme'}>
            <Button variant="plain" aria-label="Toggle theme" onClick={toggleTheme}>
              {isDark ? <SunIcon /> : <MoonIcon />}
            </Button>
          </Tooltip>
        </ToolbarItem>
        <ToolbarItem>
          {guiConfig.version === 'API NOT LOADED' ? 'API NOT LOADED' : <AppInfo />}
        </ToolbarItem>
        <ToolbarItem>
          <LoginAvatar />
        </ToolbarItem>
      </ToolbarContent>
    </Toolbar>
  );

  const masthead = (
    <Masthead display={{ default: 'inline' }}>
      <MastheadMain style={{ display: 'flex', alignItems: 'center' }}>
        <MastheadToggle>
          <PageToggleButton
            variant="plain"
            aria-label="Global navigation"
            isNavOpen={isMobileView ? isNavOpenMobile : isNavOpen}
            onNavToggle={isMobileView ? onNavToggleMobile : onNavToggle}
            id="nav-toggle"
          >
            <BarsIcon />
          </PageToggleButton>
        </MastheadToggle>
        <MastheadBrand style={{ display: 'flex', alignItems: 'center' }}>
          <MastheadLogo component="a" href="/">{guiConfig.app_name}</MastheadLogo>
        </MastheadBrand>
      </MastheadMain>
      <MastheadContent>{headerToolbar}</MastheadContent>
    </Masthead>
  );

  const Navigation = (
    <UserContext.Consumer>
      {(user: UserState) => (
        <Nav id="nav-primary-simple" aria-label="Primary navigation">
          <NavList id="nav-list-simple">
            <NavItem id="dashboard-link" itemId={'dashboard'}>
              <NavLink to="/dashboard" className={navLinkClassName}>
                Dashboard
              </NavLink>
            </NavItem>
            <Divider component="li" />
            {hasRole('Analysis') && (
              <NavItem id="exclusion-contour-link" itemId={'exclusion-contour'}>
                <NavLink to="/exclusion-zone" className={navLinkClassName}>
                  Exclusion Zone Analysis
                </NavLink>
              </NavItem>
            )}
            {hasRole('Analysis') && (
              <NavItem id="heat-map-link" itemId={'heat-map'}>
                <NavLink to="/heat-map" className={navLinkClassName}>
                  Heat Map Analysis
                </NavLink>
              </NavItem>
            )}
            {hasRole('AP') && (
              <NavItem id="mobile-ap-link" itemId={'mobile-ap'}>
                <NavLink to="/mobile-ap" className={navLinkClassName}>
                  Mobile AP
                </NavLink>
              </NavItem>
            )}
            {(hasRole('AP') || hasRole('Trial')) && (
              <NavItem id="ap-afc-link" itemId={'ap-afc'}>
                <NavLink to="/ap-afc" className={navLinkClassName}>
                  Virtual AP
                </NavLink>
              </NavItem>
            )}
            {(hasRole('AP') || hasRole('Analysis') || hasRole('Admin')) && (
              <NavItem id="AFCConfig-link" itemId={'afc-config'}>
                <NavLink to="/afc-config" className={navLinkClassName}>
                  AFC Config
                </NavLink>
              </NavItem>
            )}
            {hasRole('Admin') && <Divider component="li" />}
            {hasRole('Admin') && (
              <NavItem id="conversion-link" itemId={'conversion'}>
                <NavLink to="/convert" className={navLinkClassName}>
                  File Conversion
                </NavLink>
              </NavItem>
            )}
            {hasRole('Analysis') && (
              <NavItem id="uls-db-link" itemId="uls-db-link-item">{uls}</NavItem>
            )}
            {hasRole('Analysis') && (
              <NavItem id="antenna-link" itemId="antenna-link-item">{antenna}</NavItem>
            )}
            {hasRole('Analysis') && (
              <NavItem id="history-link" itemId="history-link-item">{history}</NavItem>
            )}
            {isAdmin() && guiConfig.grafana_enabled && <Divider component="li" />}
            {isAdmin() && guiConfig.grafana_enabled && (
              <NavItem id="grafana-link" itemId="grafana-link-item">
                <a target="_blank" rel="noopener noreferrer" href="/fbrat/grafana/">Grafana</a>
              </NavItem>
            )}
            {isAdmin() && guiConfig.grafana_enabled && (
              <NavItem id="prometheus-link" itemId="prometheus-link-item">
                <a target="_blank" rel="noopener noreferrer" href="/fbrat/prometheus/">Prometheus</a>
              </NavItem>
            )}
            {isAdmin() && guiConfig.grafana_enabled && (
              <NavItem id="cadvisor-link" itemId="cadvisor-link-item">
                <a target="_blank" rel="noopener noreferrer" href="/fbrat/cadvisor/">cAdvisor</a>
              </NavItem>
            )}
            {isAdmin() && guiConfig.grafana_enabled && (
              <NavItem id="rabbitmq-link" itemId="rabbitmq-link-item">
                <a target="_blank" rel="noopener noreferrer" href="/fbrat/rabbitmq/">RabbitMQ</a>
              </NavItem>
            )}
            {isAdmin() && guiConfig.grafana_enabled && (
              <NavItem id="kafka-ui-link" itemId="kafka-ui-link-item">
                <a target="_blank" rel="noopener noreferrer" href="/fbrat/kafka-ui/">Kafka UI</a>
              </NavItem>
            )}
            {!isLoggedIn() && showAbout() && (
              <NavItem id="about-link" itemId="about-link-item">
                <NavLink to="/about" className={navLinkClassName}>About</NavLink>
              </NavItem>
            )}
          </NavList>
        </Nav>
      )}
    </UserContext.Consumer>
  );

  const Sidebar = (
    <PageSidebar isSidebarOpen={isMobileView ? isNavOpenMobile : isNavOpen}>
      <PageSidebarBody>{Navigation}</PageSidebarBody>
    </PageSidebar>
  );

  const PageSkipToContent = <SkipToContent href="#main-content-page-layout-default-nav">Skip to Content</SkipToContent>;

  return (
    <Page masthead={masthead} sidebar={Sidebar} onPageResize={onPageResize} skipToContent={PageSkipToContent}>
      {children}
    </Page>
  );
};

export { AppLayout };
