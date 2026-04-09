import React from 'react';
import { Navigate, Route, Routes } from 'react-router-dom';
import { DynamicImport } from './DynamicImport';
import { NotFound } from './NotFound/NotFound';
import { Dashboard } from './Dashboard/Dashboard';
import { PageSection, Card, CardBody } from '@patternfly/react-core';
import {
  getAfcConfigFile,
  getAllowedRanges,
  getRegions,
  getAboutAfc,
  getAboutSiteKey,
  getRulesetIds,
} from './Lib/RatApi';
import { getUlsFiles, getAntennaPatterns, getUlsFilesCsv } from './Lib/FileApi';
import AppLoginPage from './AppLayout/AppLogin';
import { UserAccountPage } from './UserAccount/UserAccount';
import { getUsers, getMinimumEIRP } from './Lib/Admin';
import { Replay } from './Replay/Replay';
import { getLastUsedRegionFromCookie } from './Lib/Utils';
import { hasRole, isLoggedIn } from './Lib/User';

const LoadingCard = () => (
  <PageSection>
    <Card>
      <CardBody>Loading...</CardBody>
    </Card>
  </PageSection>
);

const getSupportModuleAsync = () => {
  return () => import(/* webpackChunkName: "support" */ './Support/Support');
};
const Support = () => {
  return (
    <DynamicImport load={getSupportModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <LoadingCard /> : <Component.Support />;
      }}
    </DynamicImport>
  );
};

const getRatAfcModuleAsync = () => {
  return () => import(/* webpackChunkName: "ap-afc" */ './RatAfc/RatAfc');
};
const ratAfcResolves = async () => ({
  conf: await getAfcConfigFile(getLastUsedRegionFromCookie()),
  limit: await getMinimumEIRP(),
  rulesetIds: await getRulesetIds(),
});

const RatAfc = () => {
  return (
    <DynamicImport load={getRatAfcModuleAsync()} resolve={ratAfcResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? (
          <LoadingCard />
        ) : (
          <Component.RatAfc afcConfig={resolve.conf} limit={resolve.limit} rulesetIds={resolve.rulesetIds} />
        );
      }}
    </DynamicImport>
  );
};

const getAboutAfcModuleAsync = () => {
  return () => import(/* webpackChunkName: "about" */ './About/About');
};

const ratAboutAfcResolves = async () => ({
  content: await getAboutAfc(),
  sitekey: getAboutSiteKey(),
});

const About = () => {
  return (
    <DynamicImport load={getAboutAfcModuleAsync()} resolve={ratAboutAfcResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? (
          <LoadingCard />
        ) : (
          <Component.About content={resolve.content} sitekey={resolve.sitekey} />
        );
      }}
    </DynamicImport>
  );
};

const getMobileAPModuleAsync = () => {
  return () => import(/* webpackChunkName: "mobile-ap" */ './MobileAP/MobileAP');
};
const MobileAP = () => {
  return (
    <DynamicImport load={getMobileAPModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <LoadingCard /> : <Component.MobileAP />;
      }}
    </DynamicImport>
  );
};

const getAfcConfigModuleAsync = () => {
  return () => import(/* webpackChunkName: "afcconfig" */ './AFCConfig/AFCConfig');
};

const afcConfigResolves = async () => {
  const lastRegFromCookie = getLastUsedRegionFromCookie();

  return {
    conf: await getAfcConfigFile(lastRegFromCookie!),
    ulsFiles: await getUlsFiles(),
    antennaPatterns: await getAntennaPatterns(),
    regions: await getRegions(),
    limit: await getMinimumEIRP(),
    frequencyBands: await getAllowedRanges(),
  };
};
const AFCConfig = () => {
  return (
    <DynamicImport load={getAfcConfigModuleAsync()} resolve={afcConfigResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? (
          <LoadingCard />
        ) : (
          <Component.AFCConfig
            limit={resolve.limit}
            ulsFiles={resolve.ulsFiles}
            afcConf={resolve.conf}
            antennaPatterns={resolve.antennaPatterns}
            regions={resolve.regions}
            frequencyBands={resolve.frequencyBands}
          />
        );
      }}
    </DynamicImport>
  );
};

const getConvertModuleAsync = () => {
  return () => import(/* webpackChunkName: "convert" */ './Convert/Convert');
};
const convertResolves = async () => ({
  ulsFilesCsv: await getUlsFilesCsv(),
});
const Convert = () => {
  return (
    <DynamicImport load={getConvertModuleAsync()} resolve={convertResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <LoadingCard /> : <Component.Convert ulsFilesCsv={resolve.ulsFilesCsv} />;
      }}
    </DynamicImport>
  );
};

const getExclusionZoneModuleAsync = () => {
  return () => import(/* webpackChunkName: "exclusionZone" */ './ExclusionZone/ExclusionZone');
};
const limitResolves = async () => ({
  limit: await getMinimumEIRP(),
});
const ExclusionZone = () => {
  return (
    <DynamicImport load={getExclusionZoneModuleAsync()} resolve={limitResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <LoadingCard /> : <Component.ExclusionZone limit={resolve.limit} />;
      }}
    </DynamicImport>
  );
};

const heatMapResolves = async () => ({
  limit: await getMinimumEIRP(),
  rulesetIds: await getRulesetIds(),
});
const getHeatMapModuleAsync = () => {
  return () => import(/* webpackChunkName: "heatMap" */ './HeatMap/HeatMap');
};
const HeatMap = () => {
  return (
    <DynamicImport load={getHeatMapModuleAsync()} resolve={heatMapResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? (
          <LoadingCard />
        ) : (
          <Component.HeatMap limit={resolve.limit} rulesetIds={resolve.rulesetIds} />
        );
      }}
    </DynamicImport>
  );
};

const getAdminModuleAsync = () => {
  return () => import(/* webpackChunkName: "admin" */ './Admin/Admin');
};
const adminResolves = async () => ({
  users: await getUsers(),
  limit: await getMinimumEIRP(),
  frequencyBands: await getAllowedRanges(),
  regions: await getRegions(),
});
const Admin = () => {
  return (
    <DynamicImport load={getAdminModuleAsync()} resolve={adminResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? (
          <LoadingCard />
        ) : (
          <Component.Admin
            users={resolve.users}
            limit={resolve.limit}
            frequencyBands={resolve.frequencyBands}
            regions={resolve.regions}
          />
        );
      }}
    </DynamicImport>
  );
};

const getMTLSModuleAsync = () => {
  return () => import(/* webpackChunkName: "mtlsList" */ './MTLS/MTLS');
};

const getDRListModuleAsync = () => {
  return () => import(/* webpackChunkName: "drList" */ './DeniedRules/DRList');
};

const MTLSPage = () => {
  return (
    <DynamicImport load={getMTLSModuleAsync()}>
      {(Component: any) => {
        return Component === null ? <LoadingCard /> : <Component.MTLSPage />;
      }}
    </DynamicImport>
  );
};

const drResolves = async () => ({
  regions: await getRegions(),
});

const DRListPage = () => {
  return (
    <DynamicImport load={getDRListModuleAsync()} resolve={drResolves()}>
      {(Component: any, resolve) => {
        return Component === null ? <LoadingCard /> : <Component.DRListPage regions={resolve.regions} />;
      }}
    </DynamicImport>
  );
};

export interface IAppRoute {
  label: string;
  component: React.ComponentType<any>;
  icon: any;
  path: string;
}

const routes: IAppRoute[] = [
  { component: Dashboard, icon: null, label: 'Dashboard', path: '/dashboard' },
  { component: AppLoginPage, icon: null, label: 'Login', path: '/login' },
  { component: Support, icon: null, label: 'Support', path: '/support' },
  { component: UserAccountPage, icon: null, label: 'Account', path: '/account' },
  { component: About, icon: null, label: 'About AFC', path: '/about' },
];

const AppRoutes = () => (
  <Routes>
    {routes.map(({ path, component: Component }, idx) => (
      <Route path={path} element={<Component />} key={idx} />
    ))}

    <Route path="/ap-afc" element={isLoggedIn() && (hasRole('Trial') || hasRole('AP')) ? <RatAfc /> : <Dashboard />} />
    <Route path="/mobile-ap" element={isLoggedIn() && hasRole('AP') ? <MobileAP /> : <Dashboard />} />
    <Route
      path="/afc-config"
      element={
        isLoggedIn() && (hasRole('AP') || hasRole('Analysis') || hasRole('Admin')) ? <AFCConfig /> : <Dashboard />
      }
    />
    <Route path="/convert" element={isLoggedIn() && hasRole('Admin') ? <Convert /> : <Dashboard />} />
    <Route path="/exclusion-zone" element={isLoggedIn() && hasRole('Analysis') ? <ExclusionZone /> : <Dashboard />} />
    <Route path="/heat-map" element={isLoggedIn() && hasRole('Analysis') ? <HeatMap /> : <Dashboard />} />
    <Route path="/admin" element={isLoggedIn() && hasRole('Admin') ? <Admin /> : <Dashboard />} />
    <Route path="/mtls" element={isLoggedIn() && hasRole('Admin') ? <MTLSPage /> : <Dashboard />} />
    <Route path="/deniedRules" element={isLoggedIn() && hasRole('Admin') ? <DRListPage /> : <Dashboard />} />
    <Route path="/replay" element={<Replay />} />
    <Route path="/" element={<Navigate to="/dashboard" replace />} />
    <Route path="/www" element={<Navigate to="/dashboard" replace />} />
    <Route path="*" element={<Dashboard />} />
  </Routes>
);

export { AppRoutes, routes };
