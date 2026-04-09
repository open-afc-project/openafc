import React from 'react';
import { NavLink } from 'react-router-dom';
import { Alert, PageSection } from '@patternfly/react-core';

const NotFound: React.FunctionComponent = () => {
  return (
    <PageSection>
      <Alert variant="danger" title="404! This page does not exist." />
      <br />
      <NavLink to="/dashboard">Back to Dashboard</NavLink>
    </PageSection>
  );
};

export { NotFound };
