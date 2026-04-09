/**
 * Portions copyright © 2022 Broadcom. All rights reserved.
 * The term "Broadcom" refers solely to the Broadcom Inc. corporate
 * affiliate that owns the software below.
 * This work is licensed under the OpenAFC Project License, a copy
 * of which is included with this software program.
 */

import React from 'react';
import {
  Card,
  CardBody,
  PageSection,
  FormGroup,
  TextInput,
  Title,
  Button,
  Alert,
  AlertActionCloseButton,
  InputGroup,
  InputGroupItem,
  Checkbox,
} from '@patternfly/react-core';
import { error, success } from '../Lib/RatApiTypes';
import { getUser, updateUser } from '../Lib/Admin';
import { logger } from '../Lib/Logger';
import { UserContext, UserState, isAdmin, isEditCredential } from '../Lib/User';

interface UserAccountProps {
  userId: number;
  onSuccess?: () => void;
}

interface UserAccountState {
  userId: number;
  email?: string;
  password?: string;
  passwordConf?: string;
  active: boolean;
  messageType?: 'danger' | 'success';
  messageValue: string;
  editCredential: boolean;
}

export class UserAccount extends React.Component<UserAccountProps, UserAccountState> {
  constructor(props: UserAccountProps) {
    super(props);

    this.state = {
      userId: 0,
      active: false,
      messageValue: '',
      email: '',
      password: '',
      passwordConf: '',
      editCredential: false,
    };

    getUser(props.userId).then((res) =>
      res.kind === 'Success'
        ? this.setState({
            email: res.result.email,
            active: res.result.active,
            messageType: undefined,
            messageValue: '',
            editCredential: isEditCredential(),
          } as UserAccountState)
        : this.setState({
            messageType: 'danger',
            messageValue: res.description,
          }),
    );
  }

  private upperLower = (p: string) => /[A-Z]/.test(p) && /[a-z]/.test(p);
  private hasSymbol = (p: string) => /[-!$%^&*()_+|~=`{}\[\]:";'<>@#\)\(\{\}?,.\/\\]/.test(p);

  private validEmail = (e?: string) => {
    if (!this.state.editCredential) return true;
    return !!e && /(\w+)@(\w+).(\w+)/.test(e);
  };

  private validPass = (p?: string) => {
    if (!this.state.editCredential) return true;
    if (!p) return false;
    if (p.length < 8) return false;
    if (!this.upperLower(p)) return false;
    if (!/\d/.test(p)) return false;
    if (!this.hasSymbol(p)) return false;
    return true;
  };

  private validPassConf = (p?: string) => {
    if (!this.state.editCredential) return true;
    return p === this.state.password;
  };

  private updateUser = () => {
    if (this.state.editCredential) {
      if (!this.validEmail(this.state.email)) {
        this.setState({ messageType: 'danger', messageValue: 'Invalid email' });
        return;
      }
      if (!this.validPass(this.state.password)) {
        this.setState({
          messageType: 'danger',
          messageValue:
            'Invalid password:\n Password must contain: minimum 8 characters, a number, upper and lower case letters, and a special character.',
        });
        return;
      }
      if (!this.validPassConf(this.state.passwordConf)) {
        this.setState({ messageType: 'danger', messageValue: 'Passwords must match' });
        return;
      }
    }

    logger.info('Editing user: ', this.state.userId);
    updateUser({
      id: this.props.userId,
      email: this.state.email!,
      active: this.state.active,
      password: this.state.password!,
      editCredential: this.state.editCredential,
    }).then((res) => {
      if (res.kind === 'Success') {
        if (this.props.onSuccess) {
          this.props.onSuccess();
          return;
        }
        this.setState({ messageType: 'success', messageValue: 'User updated' });
      } else {
        this.setState({ messageType: 'danger', messageValue: res.description });
      }
    });
  };

  render = () => {
    return (
      <Card>
        <CardBody>
          {this.state.messageType && (
            <>
              <Alert
                variant={this.state.messageType}
                title={this.state.messageValue || ''}
                actionClose={<AlertActionCloseButton onClose={() => this.setState({ messageType: undefined })} />}
              />
              <br />
            </>
          )}
          {this.state.editCredential ? (
            <FormGroup label="Email" fieldId="form-email">
              <InputGroup>
                <InputGroupItem isFill>
                  <TextInput
                    value={this.state.email}
                    onChange={(_event, x) => this.setState({ email: x })}
                    type="email"
                    id="form-email"
                    name="form-email"
                    validated={this.validEmail(this.state.email) ? 'default' : 'error'}
                  />
                </InputGroupItem>
              </InputGroup>
            </FormGroup>
          ) : (
            <FormGroup label="Email" fieldId="form-email">
              <InputGroup>
                <InputGroupItem isFill>
                  <TextInput
                    readOnlyVariant="default"
                    value={this.state.email}
                    type="email"
                    id="form-email"
                    name="form-email"
                  />
                </InputGroupItem>
              </InputGroup>
            </FormGroup>
          )}
          {this.state.editCredential && (
            <FormGroup label="Password" fieldId="form-pass">
              <InputGroup>
                <InputGroupItem isFill>
                  <TextInput
                    value={this.state.password}
                    onChange={(_event, x) => this.setState({ password: x })}
                    type="password"
                    id="form-pass"
                    name="form-pass"
                    validated={this.validPass(this.state.password) ? 'default' : 'error'}
                  />
                </InputGroupItem>
              </InputGroup>
            </FormGroup>
          )}

          {this.state.editCredential && (
            <FormGroup label="Confirm Password" fieldId="form-pass-c">
              <InputGroup>
                <InputGroupItem isFill>
                  <TextInput
                    value={this.state.passwordConf}
                    onChange={(_event, x) => this.setState({ passwordConf: x })}
                    type="password"
                    id="form-pass-c"
                    name="form-pass-c"
                    validated={this.validPassConf(this.state.passwordConf) ? 'default' : 'error'}
                  />
                </InputGroupItem>
              </InputGroup>
            </FormGroup>
          )}

          <br />
          <UserContext.Consumer>
            {(user: UserState) =>
              isAdmin() && (
                <>
                  <Checkbox
                    label="Is Active"
                    aria-label="User is active checkbox"
                    id="user-active-check"
                    name="user-active-check"
                    isChecked={this.state.active}
                    onChange={(_event, c: boolean) => this.setState({ active: c })}
                  />
                  <br />
                </>
              )
            }
          </UserContext.Consumer>
          <Button key="submit" variant="primary" onClick={() => this.updateUser()}>
            Update
          </Button>
        </CardBody>
      </Card>
    );
  };
}

export const UserAccountPage = () => (
  <PageSection id="ap-list-page">
    <Title headingLevel="h1">Edit User</Title>
    <UserContext.Consumer>
      {(u: UserState) => u.data.loggedIn && <UserAccount userId={u.data.userId} />}
    </UserContext.Consumer>
  </PageSection>
);
