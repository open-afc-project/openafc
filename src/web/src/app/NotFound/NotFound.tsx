import * as React from 'react';
import { NavLink } from 'react-router-dom';
import { Alert, PageSection } from '@patternfly/react-core';

// NotFound.tsx: 404 page
// author: Sam Smucny

const NotFound: React.FunctionComponent = () => {
  return (
    <PageSection>
      <Alert variant="danger" title="404! This page does not exist." />
      <br />
      <NavLink to="/dashboard" className="pf-c-nav__link">
        Back to Dashboard
      </NavLink>
    </PageSection>
  );
};

export { NotFound };
