import * as React from "react";
import "@patternfly/react-core/dist/styles/base.css";
import { NavLink, withRouter } from "react-router-dom";
import {
  Nav,
  NavList,
  NavItem,
  NavItemSeparator,
  NavVariants,
  Page,
  PageHeader,
  PageSidebar,
  SkipToContent,
  Button,
  Avatar
} from "@patternfly/react-core";
import "@app/app.css";
import { guiConfig } from "../Lib/RatApi";
import { AppInfo } from "./AppInfo";
import { LoginAvatar } from "./LoginAvatar"
import { UserContext, isAdmin, UserState, hasRole, isLoggedIn } from "../Lib/User";

/**
 * AppLayout.tsx: defines navigation regions and buttons on app (sidebar/ header)
 * author: Sam Smucny
 */

/**
 * Interface definition common to app layouts
 */
interface IAppLayout {
  children: React.ReactNode;
}

/**
 * Wrapper component used to render navigation
 * @param children Interior components to render 
 */
const AppLayout: React.FunctionComponent<IAppLayout> = ({ children }) => {
  const logoProps = {
    href: "/",
    target: "_self"
  };
  const [isNavOpen, setIsNavOpen] = React.useState(true);
  const [isMobileView, setIsMobileView] = React.useState(true);
  const [isNavOpenMobile, setIsNavOpenMobile] = React.useState(false);
  const onNavToggleMobile = () => {
    setIsNavOpenMobile(!isNavOpenMobile);
  };
  const onNavToggle = () => {
    setIsNavOpen(!isNavOpen);
  }
  const onPageResize = (props: { mobileView: boolean; windowSize: number }) => {
    setIsMobileView(props.mobileView);
  };

  const topNav = (
    <Nav>
      <NavList variant={NavVariants.horizontal}>
        <UserContext.Consumer>{(u: UserState) => isLoggedIn() ?
          <NavItem id="account-link" itemId="account-link">
            <NavLink to="/account" activeClassName="pf-m-current">Account</NavLink>
          </NavItem> : <NavItem />}
        </UserContext.Consumer>
        <UserContext.Consumer>{(u: UserState) => isAdmin() ?
          <NavItem id="admin-link" itemId="admin-link">
            <NavLink to="/admin" activeClassName="pf-m-current">Administrator</NavLink>
          </NavItem> : <NavItem />}
        </UserContext.Consumer>
        <UserContext.Consumer>{(u: UserState) => hasRole("AP") ?
          <NavItem id="ap-list-link" itemId="ap-list-link">
            <NavLink to="/ap-list" activeClassName="pf-m-current">Access Points</NavLink>
          </NavItem> : <NavItem />}
        </UserContext.Consumer>

        <UserContext.Consumer>{(u: UserState) => isAdmin() ?
          <NavItem id="mtls-link" itemId="mtls-link">
            <NavLink to="/mtls" activeClassName="pf-m-current">MTLS</NavLink>
          </NavItem> : <NavItem />}
        </UserContext.Consumer>
      </NavList>
    </Nav>
  )

  const Header = (
    <PageHeader
      logo="RLAN AFC Tool"
      logoProps={logoProps}
      topNav={topNav}
      toolbar={guiConfig.version === "API NOT LOADED" ? "API NOT LOADED" : <AppInfo />}
      showNavToggle={true}
      isNavOpen={isNavOpen}
      onNavToggle={isMobileView ? onNavToggleMobile : onNavToggle}
      avatar={<LoginAvatar />}
    />
  );

  // @ts-ignore
  const uls: any = <a target="_blank" href={guiConfig.uls_url}>ULS Databases</a>;
  // @ts-ignore
  const antenna: any = <a target="_blank" href={guiConfig.antenna_url}>Antenna Patterns</a>;
  // @ts-ignore
  const history: any = <a target="_blank" href={guiConfig.history_url}>Debug Files</a>;
  // @ts-ignore

  const showAbout = () => guiConfig.about_url;

  const Navigation = (<UserContext.Consumer>{user =>
    <Nav id="nav-primary-simple">
      <NavList id="nav-list-simple" variant={NavVariants.simple}>
        <NavItem
          id="dashboard-link"
          itemId={"dashboard"}>
          <NavLink
            to="/dashboard"
            activeClassName="pf-m-current">Dashboard</NavLink>
        </NavItem>
        <NavItemSeparator />
        {hasRole("Analysis") &&
          <NavItem
            id="exclusion-contour-link"
            itemId={"exclusion-contour"}>
            <NavLink
              to="/exclusion-zone"
              activeClassName="pf-m-current">Exclusion Zone Analysis</NavLink>
          </NavItem>}
        {hasRole("Analysis") &&
          <NavItem
            id="heat-map-link"
            itemId={"heat-map"}>
            <NavLink
              to="/heat-map"
              activeClassName="pf-m-current">Heat Map Analysis</NavLink>
          </NavItem>}
        {hasRole("AP") &&
          <NavItem
            id="mobile-ap-link"
            itemId={"mobile-ap"}>
            <NavLink
              to="/mobile-ap"
              activeClassName="pf-m-current">Mobile AP</NavLink>
          </NavItem>}
        {(hasRole("AP") || hasRole("Trial")) &&
          <NavItem
            id="ap-afc-link"
            itemId={"ap-afc"}>
            <NavLink
              to="/ap-afc"
              activeClassName="pf-m-current">Virtual AP</NavLink>
          </NavItem>}
        {(hasRole("AP") || hasRole("Analysis")) &&
          <NavItem
            id="AFCConfig-link"
            itemId={"afc-config"}>
            <NavLink
              to="/afc-config"
              activeClassName="pf-m-current">AFC Config</NavLink>
          </NavItem>}
        {hasRole("Admin") &&
          <NavItemSeparator />}
        {hasRole("Admin") &&
          <NavItem
            id="conversion-link"
            itemId={"conversion"}>
            <NavLink
              to="/convert"
              activeClassName="pf-m-current">File Conversion</NavLink>
          </NavItem>}
        {hasRole("Analysis") &&
          <NavItem
            id="uls-db-link"
            itemId="uls-db-link-item">
            {uls}
          </NavItem>}
        {hasRole("Analysis") &&
          <NavItem
            id="antenna-link"
            itemId="antenna-link-item">
            {antenna}
          </NavItem>}
        {hasRole("Analysis") &&
          <NavItem
            id="history-link"
            itemId="history-link-item">
            {history}
          </NavItem>}
        {!isLoggedIn() && showAbout() &&
          <NavItem
            id="about-link"
            itemId="about-link-item">
            <NavLink
              to="/about"
              activeClassName="pf-m-current">About</NavLink>
          </NavItem>}
      </NavList>
    </Nav>}
  </UserContext.Consumer>);
  const Sidebar = (
    <PageSidebar
      nav={Navigation}
      isNavOpen={isMobileView ? isNavOpenMobile : isNavOpen}
    />
  );
  const PageSkipToContent = (
    <SkipToContent href="#main-content-page-layout-default-nav">
      Skip to Content
    </SkipToContent>
  );
  return (
    <Page
      header={Header}
      sidebar={Sidebar}
      onPageResize={onPageResize}
      skipToContent={PageSkipToContent}>
      {children}
    </Page>
  );
}

export { AppLayout };
