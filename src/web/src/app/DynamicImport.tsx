import * as React from "react";

/**
 * DynamicImport.tsx: component which loads app modules from the server on demand
 * author: Sam Smucny
 */

/**
 * Interface definition for `IDynamicImport`
 * @member load promise whose result is the page bundle with code needed to render children. Result is passed to the children rendering function
 * @member children interior elements to render
 * @member resolve promise object that is resoved before children are loaded. Once loaded the result of the promise can be accessed to construct the children
 */
interface IDynamicImport<T = undefined> {
  load: () => Promise<any>;
  children: (component: any, resolved: T) => JSX.Element,  
  resolve?: Promise<T>
};

/**
 * Class to dynamically load bundles from server
 * this decreases the transfer size of individual parts
 * also supports general resolves (can be used for API calls) when certain resources need to be
 * loaded before a component is mounted
 * @typeparam T (optional) type of resolve object
 */
class DynamicImport<T> extends React.Component<IDynamicImport<T>> {
  public state = {
    component: null,
    resolve: undefined
  }
  public componentDidMount() {
    (this.props.resolve !== undefined ? this.props.resolve : Promise.resolve(undefined as unknown as T))
      .then(resolve =>
        this.props.load().then((component) => {
          this.setState({
            component: component.default ? component.default : component,
            resolve: resolve
          });
        })
      );
  }
  public render() {
    return this.props.children(this.state.component, this.state.resolve)
  }
}

export { DynamicImport };
