import React from 'react';
import { PageSection, Card, CardBody, Alert } from '@patternfly/react-core';

interface IDynamicImport<T = undefined> {
  load: () => Promise<any>;
  children: (component: any, resolved: T) => JSX.Element;
  resolve?: Promise<T>;
}

class ErrorBoundary extends React.Component<{ children: React.ReactNode }, { hasError: boolean; error?: Error }> {
  constructor(props: { children: React.ReactNode }) {
    super(props);
    this.state = { hasError: false };
  }

  static getDerivedStateFromError(error: Error) {
    return { hasError: true, error };
  }

  render() {
    if (this.state.hasError) {
      return (
        <PageSection>
          <Card>
            <CardBody>
              <Alert variant="danger" title="Page failed to load">
                {this.state.error?.message || 'An unexpected error occurred while rendering this page.'}
              </Alert>
            </CardBody>
          </Card>
        </PageSection>
      );
    }
    return this.props.children;
  }
}

class DynamicImport<T> extends React.Component<IDynamicImport<T>> {
  public state = {
    component: null,
    resolve: undefined,
    loadError: undefined as string | undefined,
  };
  public componentDidMount() {
    (this.props.resolve !== undefined ? this.props.resolve : Promise.resolve(undefined as unknown as T))
      .then((resolve) =>
        this.props.load().then((component) => {
          this.setState({
            component: component.default ? component.default : component,
            resolve: resolve,
          });
        }),
      )
      .catch((err) => {
        this.setState({ loadError: String(err?.message || err) });
      });
  }
  public render() {
    if (this.state.loadError) {
      return (
        <PageSection>
          <Card>
            <CardBody>
              <Alert variant="danger" title="Failed to load page module">
                {this.state.loadError}
              </Alert>
            </CardBody>
          </Card>
        </PageSection>
      );
    }
    return <ErrorBoundary>{this.props.children(this.state.component, this.state.resolve)}</ErrorBoundary>;
  }
}

export { DynamicImport };
