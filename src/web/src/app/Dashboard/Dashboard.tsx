import * as React from 'react';
import { PageSection, Title, Card, CardBody } from '@patternfly/react-core';
import { guiConfig } from '../Lib/RatApi';

/**
 * Dashboard.tsx: application splash page. Currently empty
 * author: Sam Smucny
 */

/**
 * Page level component for dashboard. Nothing interesting now.
 */
const Dashboard: React.FunctionComponent = () => {
  return (
    <PageSection>
      <Title size={'xl'}>{guiConfig.app_name}</Title>
      <Card>
        <CardBody>Navigate between pages using the sidebar menu.</CardBody>
      </Card>
    </PageSection>
  );
};

export { Dashboard };
