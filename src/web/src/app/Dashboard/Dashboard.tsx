import React from 'react';
import { PageSection, Title, Card, CardBody } from '@patternfly/react-core';
import { guiConfig } from '../Lib/RatApi';

const Dashboard: React.FunctionComponent = () => {
  return (
    <PageSection>
      <Title headingLevel="h1">{guiConfig.app_name || 'AFC Dashboard'}</Title>
      <Card>
        <CardBody>Navigate between pages using the sidebar menu.</CardBody>
      </Card>
    </PageSection>
  );
};

export { Dashboard };
