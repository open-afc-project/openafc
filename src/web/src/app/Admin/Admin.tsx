import * as React from "react";
import { PageSection, Card, CardHead, CardBody, CardHeader, Title, Modal, Button, FormGroup, FormSelect, FormSelectOption, Alert, AlertActionCloseButton} from "@patternfly/react-core";
import { UserTable } from "./UserList";
import { Role } from "../Lib/User";
import { getUsers, addUserRole, deleteUser, removeUserRole, setMinimumEIRP, Limit } from "../Lib/Admin";
import { logger } from "../Lib/Logger";
import { APList } from "../APList/APList";
import { UserAccount } from "../UserAccount/UserAccount";
import { UserModel, RatResponse } from "../Lib/RatApiTypes";
import { element } from "prop-types";

/**
 * Admin.tsx: Administration page for managing users
 * author: Sam Smucny
 */

/**
 * Possible roles enumerated
 */
const roles: Role[] = ["AP", "Analysis", "Admin"];

/**
 * Administrator tab for managing users
 */
export class Admin extends React.Component<{ users: RatResponse<UserModel[]> }, 
{ 
  users: UserModel[],
  userId?: number
  addRoleModelOpen: boolean,
  newRole?: Role,
  removeRoleModalOpen: boolean,
  removeRole?: Role,
  deleteModalOpen: boolean,
  apModalOpen: boolean,
  userModalOpen: boolean,
  limit: Limit,
  messageSuccess?: string,
  messageError?: string
}> {
  constructor(props: Readonly<{ users: RatResponse<UserModel[]>; limit: RatResponse<Limit>;}>) {
    super(props);
    
    if (props.users.kind === "Error")
      logger.error(props.users);
  
    const userList = props.users.kind === "Success" ? props.users.result : [];
    const apiLimit = props.limit.kind === "Success" ? props.limit.result : new Limit(false, 18);

    this.state = { 
      users: userList,
      addRoleModelOpen: false,
      removeRoleModalOpen: false,
      deleteModalOpen: false,
      apModalOpen: false,
      userModalOpen: false,
      limit: apiLimit,
      messageSuccess: undefined,
      messageError: undefined
    };
  }


  /**
   * Delete the user with `this.state.userId`
   */
  private deleteUser() {
    logger.info("Deleting user: ", this.state.userId);
    if (!this.state.userId) return;
    deleteUser(this.state.userId).then(
      async res => {
        if (res.kind === "Success") {
          const users = await getUsers();
          if (users.kind === "Error") {
            logger.error(users);
            return;
          }
          this.handleDeleteModalToggle();
          this.setState({ users: users.result, deleteModalOpen: false });
        } else {
          logger.error(res);
        }
      }
    );
  }

  /**
   * Add `this.state.newRole` to `this.state.userId`
   */
  private addRole() {
    logger.info("Adding role: ", this.state.newRole);
    if (!this.state.userId || ! this.state.newRole) return;
    addUserRole(this.state.userId, this.state.newRole).then(
      async res => {
        if (res.kind === "Success") {
          const users = await getUsers();
          if (users.kind === "Error") {
            logger.error(users);
            return;
          }
          this.setState({ users: users.result });
          this.handleAddRoleModalToggle();
        } else {
          logger.error(res);
        }
      }
    );
  }

  /**
   * Remove `this.state.removeRole` from `this.state.userId`
   */
  private removeRole() {
    logger.info("Removing role: ", this.state.removeRole);
    if (!this.state.userId || ! this.state.removeRole) return;
    removeUserRole(this.state.userId, this.state.removeRole).then(
      async res => {
        if (res.kind === "Success") {
          const users = await getUsers();
          if (users.kind === "Error") {
            logger.error(users);
            return;
          }
          this.handleRemoveRoleModalToggle();
          this.setState({ users: users.result });
        } else {
          logger.error(res);
        }
      }
    );
  }

  /**
   * Updates user data after an edit
   */
  private async userEdited() {
    const users = await getUsers();
    if (users.kind === "Error") {
      logger.error(users);
      return;
    }
    this.setState({ users: users.result });
    this.handleUserModalToggle();
  }

  private handleAddRoleModalToggle = (id?: number) => this.setState(s => ({ userId: id, addRoleModelOpen: !s.addRoleModelOpen }));
  private handleRemoveRoleModalToggle = (id?: number) => this.setState(s => ({ userId: id, removeRoleModalOpen: !s.removeRoleModalOpen }));
  private handleDeleteModalToggle = (id?: number) => this.setState(s => ({ userId: id, deleteModalOpen: !s.deleteModalOpen }));
  private handleAPModalToggle = (id?: number) => this.setState(s => ({ userId: id, apModalOpen: !s.apModalOpen }));
  private handleUserModalToggle = (id?: number) => this.setState(s => ({ userId: id, userModalOpen: !s.userModalOpen }));
  private handleMinEIRP = (minEIRP: number) => this.setState({ limit: {...this.state.limit, limit: minEIRP} });
  private submitMinEIRP = (limit: number | boolean) => {
    setMinimumEIRP(limit).then((res) => {
      let successMessage = limit === false ? "Minimum EIRP removed." : "Minimum EIRP set to " + limit + " dBm.";
      if(res.kind == "Success") {
        this.setState({messageSuccess: successMessage, messageError: undefined});
      } else {
        this.setState({messageError: "Unable to update limits.", messageSuccess: undefined});
      }
    })
  }

  render() {
    return (
      <PageSection>
        <Card>
          <CardHead><CardHeader>Limits</CardHeader></CardHead>
            {this.state.limit.enforce ? 
                <CardBody>
                  <input id="limitEnabled" type="checkbox" checked={this.state.limit.enforce} onChange={(e) => this.setState({limit: {...this.state.limit, enforce: e.target.checked}})}/>
                  <label htmlFor="limitEnabled">Use Minimum EIRP value</label>
                  <br/>
                  <label htmlFor="min_EIRP">Minimum EIRP value</label>
                  <br/>
                  <input value={this.state.limit.limit} onChange={(event) => this.handleMinEIRP(Number(event.target.value))} id="min_EIRP" type="number"/>dBm
                  <Button style={{marginLeft: "10px"}} onClick={() => this.submitMinEIRP(this.state.limit.limit)}> Save</Button>
              </CardBody> : 
              
              <CardBody>
                <input id="limitEnabled" type="checkbox" checked={this.state.limit.enforce} onChange={(e) => this.setState({limit: {...this.state.limit, enforce: e.target.checked}})}/>
                <label htmlFor="limitEnabled">Use Minimum EIRP value</label>
                <br/>
                <Button onClick={() => this.submitMinEIRP(false)}> Save</Button>
              </CardBody>
            }
        </Card>
        <br/>
            <>
              {this.state.messageError !== undefined && (
                  <Alert
                      variant="danger"
                      title="Error"
                      action={<AlertActionCloseButton onClose={() => this.setState({ messageError: undefined })} />}
                  >{this.state.messageError}</Alert>
              )}
            </>
            <>
              {this.state.messageSuccess !== undefined && (
                  <Alert
                      variant="success"
                      title="Success"
                      action={<AlertActionCloseButton onClose={() => this.setState({ messageSuccess: undefined })} />}
                  >{this.state.messageSuccess}</Alert>
              )}
            </>
        <br/>
        <Card>
          <CardHead>
            <CardHeader>Users</CardHeader>
          </CardHead>
          <CardBody>
            <UserTable users={this.state.users} 
              onDelete={this.handleDeleteModalToggle} 
              onRoleAdd={this.handleAddRoleModalToggle} 
              onRoleRemove={this.handleRemoveRoleModalToggle} 
              onAPEdit={this.handleAPModalToggle}
              onUserEdit={this.handleUserModalToggle}
              />
          </CardBody>
          <Modal
            isSmall={true}
            title="Add Role"
            isOpen={this.state.addRoleModelOpen}
            onClose={this.handleAddRoleModalToggle}
            actions={[
              <Button key="confirm" variant="primary" onClick={() => this.addRole()}>
                Confirm
            </Button>
            ]}
          >
            <FormGroup
              label="New Role"
              fieldId="new-role">
                <FormSelect
                  value={this.state.newRole}
                  // @ts-ignore
                  onChange={x => this.setState({ newRole: x as Role })}
                  id="new-role"
                  name="new-role"
                >
                  <FormSelectOption key={0} value={undefined} label={"Select a role"} />
                  {
                    roles.map(r => <FormSelectOption key={r} value={r} label={r} />)
                  }
                </FormSelect>
              </FormGroup>
            </Modal>
            <Modal
            isSmall={true}
            title="Remove Role"
            isOpen={this.state.removeRoleModalOpen}
            onClose={this.handleRemoveRoleModalToggle}
            actions={[
              <Button key="confirm" variant="primary" onClick={() => this.removeRole()}>
                Confirm
            </Button>
            ]}
          >
            <FormGroup
              label="Select Role"
              fieldId="remove-role">
                <FormSelect
                  value={this.state.removeRole}
                  // @ts-ignore
                  onChange={x => this.setState({ removeRole: x as Role })}
                  id="remove-role"
                  name="remove-role"
                >
                  <FormSelectOption key={0} value={undefined} label={"Select a role"} />
                  {
                    roles.map(r => <FormSelectOption key={r} value={r} label={r} />)
                  }
                </FormSelect>
              </FormGroup>
            </Modal>
            <Modal
            isSmall={true}
            title="Delete User Confimation"
            isOpen={this.state.deleteModalOpen}
            onClose={this.handleDeleteModalToggle}
            actions={[
              <Button key="confirm" variant="primary" onClick={() => this.deleteUser()}>
                Confirm
            </Button>
            ]}
          >
            Are you sure you want to delete {this.state.deleteModalOpen && this.state.users.find(x => x.id === this.state.userId)?.email}?
            </Modal>
            <Modal
            title="Edit Access Points"
            isOpen={this.state.apModalOpen}
            onClose={this.handleAPModalToggle}
          >
              <APList userId={this.state.userId!} />
            </Modal>
            <Modal
            title="Edit User"
            isOpen={this.state.userModalOpen}
            onClose={this.handleUserModalToggle}
          >
              <UserAccount userId={this.state.userId!} onSuccess={() => this.userEdited()} />
            </Modal>
        </Card>
        <br/>
          {this.state.users.length && <APList userId={0} users={this.state.users} />}{/* userId=0 means don't scope to user */}
      </PageSection>
    )
  }
}