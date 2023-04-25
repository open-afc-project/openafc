import * as React from "react";
import { Redirect, Route, RouteComponentProps, Switch } from "react-router-dom";
import { DynamicImport } from "./DynamicImport";
import { NotFound } from "./NotFound/NotFound";
import { Dashboard } from "./Dashboard/Dashboard";
import { PageSection, Card, CardHeader } from "@patternfly/react-core";
import { getAfcConfigFile, getAllowedRanges, getRegions, getAboutAfc, getAboutSiteKey } from "./Lib/RatApi";
import { getUlsFiles, getAntennaPatterns, getUlsFilesCsv } from "./Lib/FileApi";
import AppLoginPage from "./AppLayout/AppLogin";
import { UserAccountPage } from "./UserAccount/UserAccount";
import { getUsers, getMinimumEIRP, Limit, getDeniedRegions,  } from "./Lib/Admin";
import { Replay } from "./Replay/Replay"
import { getLastUsedRegionFromCookie } from "./Lib/Utils";
import { resolve } from "path";

/**
 * routes.tsx: definition of app routes
 * author: Sam Smucny
 */

// these define dynamic modules which use the DynamicImport module to load routes

/**
 * Helper
 */
const getSupportModuleAsync = () => {
  return () => import(/* webpackChunkName: "support" */ "./Support/Support");
}
const Support = () => {
  return (
    <DynamicImport load={getSupportModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection> : <Component.Support />
      }}
    </DynamicImport>
  );
}

const getRatAfcModuleAsync = () => {
  return () => import(/* webpackChunkName: "ap-afc" */ "./RatAfc/RatAfc");
}
const ratAfcResolves = async () => ({
  conf: await getAfcConfigFile(getLastUsedRegionFromCookie()),
  limit: await getMinimumEIRP()
})


const RatAfc = () => {
  return (
    <DynamicImport load={getRatAfcModuleAsync()} resolve={ratAfcResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.RatAfc afcConfig={resolve.conf} limit={resolve.limit} />
      }}
    </DynamicImport>
  );
}

const getAboutAfcModuleAsync = () => {
  return () => import(/* webpackChunkName: "about" */ "./About/About");
}

const ratAboutAfcResolves = async () => ({
  content: await getAboutAfc(),
  sitekey: getAboutSiteKey()
})

const About = () => {
  return (
    <DynamicImport load={getAboutAfcModuleAsync()} resolve={ratAboutAfcResolves()} >
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.About content={resolve.content} sitekey={resolve.sitekey}/>
      }}
    </DynamicImport>
  );
}

const getMobileAPModuleAsync = () => {
  return () => import(/* webpackChunkName: "mobile-ap" */ "./MobileAP/MobileAP");
}
const MobileAP = () => {
  return (
    <DynamicImport load={getMobileAPModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection> : <Component.MobileAP />
      }}
    </DynamicImport>
  );
}

const getVirtualAPModuleAsync = () => {
  return () => import(/* webpackChunkName: "virtualap" */ "./VirtualAP/VirtualAP");
}
const VirtualAP = () => {
  return (
    <DynamicImport load={getVirtualAPModuleAsync()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection> : <Component.VirtualAP />
      }}
    </DynamicImport>
  );
}

const getAfcConfigModuleAsync = () => {
  return () => import(/* webpackChunkName: "afcconfig" */ "./AFCConfig/AFCConfig");
}

const afcConfigResolves = async () => {
  var lastRegFromCookie = getLastUsedRegionFromCookie();


  return {
    conf: await getAfcConfigFile(lastRegFromCookie!),
    ulsFiles: await getUlsFiles(),
    antennaPatterns: await getAntennaPatterns(),
    regions: await getRegions(),
    limit: await getMinimumEIRP(),
    frequencyBands: await getAllowedRanges()
  }
}
const AFCConfig = () => {
  return (
    <DynamicImport load={getAfcConfigModuleAsync()} resolve={afcConfigResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.AFCConfig limit={resolve.limit} ulsFiles={resolve.ulsFiles} afcConf={resolve.conf} antennaPatterns={resolve.antennaPatterns} regions={resolve.regions} frequencyBands={resolve.frequencyBands} />
      }}
    </DynamicImport>
  );
}

const getConvertModuleAsync = () => {
  return () => import(/* webpackChunkName: "convert" */ "./Convert/Convert");
}
const convertResolves = async () => ({
  ulsFilesCsv: await getUlsFilesCsv(),
})
const Convert = () => {
  return (
    <DynamicImport load={getConvertModuleAsync()} resolve={convertResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.Convert ulsFilesCsv={resolve.ulsFilesCsv} />
      }}
    </DynamicImport>
  );
}

const getExclusionZoneModuleAsync = () => {
  return () => import(/* webpackChunkName: "exclusionZone" */ "./ExclusionZone/ExclusionZone");
}
const limitResolves = async () => ({
  limit: await getMinimumEIRP()
})
const ExclusionZone = () => {
  return (
    <DynamicImport load={getExclusionZoneModuleAsync()} resolve={limitResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.ExclusionZone limit={resolve.limit} />
      }}
    </DynamicImport>
  );
}

const getHeatMapModuleAsync = () => {
  return () => import(/* webpackChunkName: "heatMap" */ "./HeatMap/HeatMap");
}
const HeatMap = () => {
  return (
    <DynamicImport load={getHeatMapModuleAsync()} resolve={limitResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.HeatMap limit={resolve.limit} />
      }}
    </DynamicImport>
  );
}

const getAdminModuleAsync = () => {
  return () => import(/* webpackChunkName: "admin" */ "./Admin/Admin");
}
const adminResolves = async () => ({
  users: await getUsers(),
  limit: await getMinimumEIRP(),
  frequencyBands: await getAllowedRanges(),
  regions: await getRegions()
})
const Admin = () => {
  return (
    <DynamicImport load={getAdminModuleAsync()} resolve={adminResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.Admin users={resolve.users} limit={resolve.limit} frequencyBands={resolve.frequencyBands} regions={resolve.regions} />
      }}
    </DynamicImport>
  );
}

const getAPListModuleAsync = () => {
  return () => import(/* webpackChunkName: "apList" */ "./APList/APList");
}

const getMTLSModuleAsync = () => {
  return () => import(/* webpackChunkName: "mtlsList" */ "./MTLS/MTLS");
}

const getDRListModuleAsync = () => {
  return () => import(/* webpackChunkName: "drList" */ "./DeniedRegions/DRList");
}



const APListPage = () => {
  return (
    <DynamicImport load={getAPListModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.APListPage />
      }}
    </DynamicImport>
  );
}

const MTLSPage = () => {
  return (
    <DynamicImport load={getMTLSModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.MTLSPage />
      }}
    </DynamicImport>
  );
}

const drResolves = async () => ({
  regions: await getRegions(),
})

const DRListPage = () => {
  return (
    <DynamicImport load={getDRListModuleAsync()} resolve={drResolves()}>
      {(Component: any) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.DRListPage />
      }}
    </DynamicImport>
  );
}



const getReplayModuleAsync = () => {
  return () => import(/* webpackChunkName: "replay" */ "./Replay/Replay");
}
const ReplayPage = () => {
  return (
    <DynamicImport load={getReplayModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <PageSection><Card><CardHeader>Loading...</CardHeader></Card></PageSection>
          : <Component.ReplayPage />
      }}
    </DynamicImport>
  );
}

export interface IAppRoute {
  label: string;
  component: React.ComponentType<RouteComponentProps<any>> | React.ComponentType<any>;
  icon: any;
  exact?: boolean;
  path: string;
}

// definition of app routes
const routes: IAppRoute[] = [
  {
    component: Dashboard, // Currently empty
    exact: true,
    icon: null,
    label: "Dashboard",
    path: "/dashboard"
  },
  {
    component: AppLoginPage, // Currently empty
    exact: true,
    icon: null,
    label: "Login",
    path: "/login"
  },
  {
    component: Support, // currently empty
    exact: true,
    icon: null,
    label: "Support",
    path: "/support"    // not pointed to anywhere
  },
  {
    component: RatAfc,
    exact: true,
    icon: null,
    label: "RAT AFC AP",
    path: "/ap-afc"
  },
  {
    component: MobileAP,
    exact: true,
    icon: null,
    label: "Mobile AP",
    path: "/mobile-ap"
  },
  {
    component: VirtualAP,
    exact: true,
    icon: null,
    label: "Virtual AP",
    path: "/virtual-ap"
  },
  {
    component: AFCConfig,
    exact: true,
    icon: null,
    label: "AFC Configuration",
    path: "/afc-config"
  },
  {
    component: Convert,
    exact: true,
    icon: null,
    label: "File Conversion",
    path: "/convert"
  },
  {
    component: ExclusionZone,
    exact: true,
    icon: null,
    label: "Exclusion Zone",
    path: "/exclusion-zone"
  },
  {
    component: HeatMap,
    exact: true,
    icon: null,
    label: "Heat Map",
    path: "/heat-map"
  },
  {
    component: Admin,
    exact: true,
    icon: null,
    label: "Administrator",
    path: "/admin"
  },
  {
    component: APListPage,
    exact: true,
    icon: null,
    label: "Access Points",
    path: "/ap-list"
  },
  {
    component: MTLSPage,
    exact: true,
    icon: null,
    label: "MTLS",
    path: "/mtls"
  },
  {
    component: DRListPage,
    exact: true,
    icon: null,
    label: "Denied Regions",
    path: "/deniedRegions"
  },
  {
    component: UserAccountPage,
    exact: true,
    icon: null,
    label: "Account",
    path: "/account"
  },
  {
    component: About,
    exact: true,
    icon: null,
    label: "About AFC",
    path: "/about"
  },
];

const AppRoutes = () => (
  <Switch>
    {routes.map(({ path, exact, component }, idx) => (
      <Route path={path} exact={exact} component={component} key={idx} />
    ))}
    <Route path="/replay" exact={true} component={Replay}></Route>
    <Redirect exact={true} from="/" to="/dashboard" />
    <Redirect exact={true} from="www" to="/dashboard" />
    <Route component={Dashboard} />
    <Route component={NotFound} />
  </Switch>
);

export { AppRoutes, routes };
